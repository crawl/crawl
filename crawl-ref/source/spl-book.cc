/*
 *  File:       spl-book.cc
 *  Summary:    Spellbook/Staff contents array and management functions
 *  Written by: Josh Fishman
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
#include "spl-book.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>

#ifdef DOS
 #include <conio.h>
#endif

#include "externs.h"
#include "cio.h"
#include "debug.h"
#include "delay.h"
#include "food.h"
#include "format.h"
#include "invent.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "it_use3.h"
#include "message.h"
#include "player.h"
#include "randart.h"
#include "religion.h"
#include "spl-cast.h"
#include "spl-mis.h"
#include "spl-util.h"
#include "state.h"
#include "stuff.h"

#define SPELL_LIST_KEY "spell_list"

#define RANDART_BOOK_TYPE_KEY  "randart_book_type"
#define RANDART_BOOK_LEVEL_KEY "randart_book_level"

#define RANDART_BOOK_TYPE_LEVEL "level"
#define RANDART_BOOK_TYPE_THEME "theme"

#define NUMBER_SPELLBOOKS 61

static spell_type spellbook_template_array[NUMBER_SPELLBOOKS][SPELLBOOK_SIZE] =
{
    // 0 - Minor Magic I
    {SPELL_MAGIC_DART,
     SPELL_SUMMON_SMALL_MAMMALS,
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
     SPELL_SUMMON_SMALL_MAMMALS,
     SPELL_BLINK,
     SPELL_REPEL_MISSILES,
     SPELL_SLOW,
     SPELL_CALL_CANINE_FAMILIAR,
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
     SPELL_CALL_CANINE_FAMILIAR,
     SPELL_SUMMON_UGLY_THING,
     SPELL_SHADOW_CREATURES,
     SPELL_SUMMON_WRAITHS,
     SPELL_SUMMON_HORRIBLE_THINGS,
     SPELL_NO_SPELL,
     },
    // 8 - Book of Fire
    {SPELL_EVAPORATE,
     SPELL_FIRE_BRAND,
     SPELL_SUMMON_ELEMENTAL,
     SPELL_BOLT_OF_MAGMA,
     SPELL_IGNITE_POISON,
     SPELL_DELAYED_FIREBALL,
     SPELL_RING_OF_FLAMES,
     SPELL_NO_SPELL,
     },
    // 9 - Book of Ice
    {SPELL_FREEZING_AURA,
     SPELL_SLEEP,
     SPELL_CONDENSATION_SHIELD,
     SPELL_OZOCUBUS_REFRIGERATION,
     SPELL_BOLT_OF_COLD,
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
     SPELL_PORTAL_PROJECTILE,
     SPELL_BLINK,
     SPELL_RECALL,
     SPELL_TELEPORT_OTHER,
     SPELL_CONTROL_TELEPORT,
     SPELL_TELEPORT_SELF,
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
     SPELL_POISON_WEAPON,
     SPELL_MEPHITIC_CLOUD,
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
     SPELL_LETHAL_INFUSION,
     SPELL_BONE_SHARDS,
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
     SPELL_PETRIFY,
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
     SPELL_DETECT_CURSE,
     SPELL_SELECTIVE_AMNESIA,
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
    // 22 - Book of Healing --  XXX: not used
    {SPELL_LESSER_HEALING,
     SPELL_CURE_POISON_I,
     SPELL_PURIFICATION,
     SPELL_GREATER_HEALING,
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
    {SPELL_SUMMON_SMALL_MAMMALS,
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
    //   already very common (i.e,. this level nine spell occurred in
    //   two books!)

    // 29 - Book of the Sky
    {SPELL_SUMMON_ELEMENTAL,
     SPELL_INSULATION,
     SPELL_AIRSTRIKE,
     SPELL_FLY,
     SPELL_SILENCE,
     SPELL_LIGHTNING_BOLT,
     SPELL_DEFLECT_MISSILES,
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
     SPELL_SUMMON_SCORPIONS,
     SPELL_POISON_AMMUNITION,
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
    {SPELL_CONTROL_TELEPORT,
     SPELL_ENSLAVEMENT,
     SPELL_TAME_BEASTS,
     SPELL_MASS_CONFUSION,
     SPELL_CONTROL_UNDEAD,
     SPELL_MASS_SLEEP,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },
    // 37 - Book of Morphology
    {SPELL_FRAGMENTATION,
     SPELL_POLYMORPH_OTHER,
     SPELL_CIGOTUVIS_DEGENERATION,
     SPELL_ALTER_SELF,
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
     SPELL_SUMMON_SMALL_MAMMALS,
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
    {SPELL_SUMMON_SMALL_MAMMALS,
     SPELL_STICKS_TO_SNAKES,
     SPELL_DETECT_CREATURES,
     SPELL_CALL_CANINE_FAMILIAR,
     SPELL_TAME_BEASTS,
     SPELL_DRAGON_FORM,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // 47 - Book of Stalking //jmf: 24jun2000
    {SPELL_STING,
     SPELL_SURE_BLADE,
     SPELL_PROJECTED_NOISE,
     SPELL_POISON_WEAPON,
     SPELL_MEPHITIC_CLOUD,
     SPELL_PETRIFY,
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
     SPELL_SUMMON_SWARM,
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
    // 54 - Rod of destruction (lightning, iron, fireball)
    {SPELL_LIGHTNING_BOLT,
     SPELL_BOLT_OF_IRON,
     SPELL_FIREBALL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },
    // 55 - Rod of destruction (inaccuracy, magma, cold)
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

spell_type which_spell_in_book(const item_def &book, int spl)
{
    ASSERT(book.base_type == OBJ_BOOKS || book.base_type == OBJ_STAVES);

    const CrawlHashTable &props = book.props;
    if (!props.exists( SPELL_LIST_KEY ))
        return which_spell_in_book(book.book_number(), spl);

    const CrawlVector &spells = props[SPELL_LIST_KEY].get_vector();

    ASSERT( spells.get_type() == SV_LONG );
    ASSERT( spells.size() == SPELLBOOK_SIZE );

    return static_cast<spell_type>(spells[spl].get_long());
}

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
        spell_type stype = which_spell_in_book(book, j);
        if (stype == SPELL_NO_SPELL)
            continue;

        out.cprintf(" ");

        bool knows_spell = false;
        for (i = 0; i < 25 && !knows_spell; i++)
            knows_spell = (you.spells[i] == stype);

        const int level_diff = spell_difficulty( stype );
        const int levels_req = spell_levels_required( stype );

        int colour = DARKGREY;
        if (action == RBOOK_USE_STAFF)
        {
            if (book.base_type == OBJ_BOOKS ?
                you.experience_level >= level_diff
                    && you.magic_points >= level_diff
                : book.plus >= level_diff * ROD_CHARGE_MULT)
            {
                colour = LIGHTGREY;
            }
        }
        else
        {
            if (knows_spell)
                colour = LIGHTGREY;
            else if (you.experience_level >= level_diff
                     && spell_levels >= levels_req
                     && spell_skills)
            {
                colour = LIGHTBLUE;
            }
        }

        out.textcolor( colour );

        char strng[2];
        strng[0] = index_to_letter(spelcount);
        strng[1] = 0;

        out.cprintf(strng);
        out.cprintf(" - ");

        out.cprintf( "%s", spell_title(stype) );
        out.cgotoxy( 35, -1 );


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

        out.cgotoxy( 65, -1 );

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

    case BOOK_HEALING: //  XXX: not used
        return 100;

    default:
        return 1;
    }
}

static unsigned char _lowest_rarity[NUM_SPELLS];

void init_spell_rarities()
{
    for (int i = 0; i < NUM_SPELLS; i++)
        _lowest_rarity[i] = 255;

    for (int i = 0; i < NUM_BOOKS; i++)
    {
        const int rarity = book_rarity(i);

        // Manuals, books of destruction, books only created as gifts
        // from specific gods, and the unused Book of Healing.
        if (rarity >= 20)
            continue;

        for (int j = 0; j < SPELLBOOK_SIZE; j++)
        {
            spell_type spell = which_spell_in_book(i, j);
            if (spell == SPELL_NO_SPELL)
                continue;

#ifdef DEBUG
            int unsigned flags = get_spell_flags(spell);

            if (flags & (SPFLAG_MONSTER | SPFLAG_TESTING))
            {
                item_def item;
                item.base_type = OBJ_BOOKS;
                item.sub_type  = i;

                end(1, false, "Spellbook '%s' contains invalid spell "
                             "'%s'",
                    item.name(DESC_PLAIN, false, true).c_str(),
                    spell_title(spell));
            }
#endif

            if (rarity < _lowest_rarity[spell])
                _lowest_rarity[spell] = rarity;
        }
    }
}

int spell_rarity(spell_type which_spell)
{
    const int rarity = _lowest_rarity[which_spell];

    if (rarity == 255)
        return (-1);

    return (rarity);
}

bool is_valid_spell_in_book( const item_def &book, int spell )
{
    return which_spell_in_book(book, spell) != SPELL_NO_SPELL;
}

bool is_valid_spell_in_book( int splbook, int spell )
{
    return which_spell_in_book(splbook, spell) != SPELL_NO_SPELL;
}

static int _which_spellbook( void )
{
    int book = -1;
    const int avail_levels = player_spell_levels();

    // Knowing delayed fireball will allow Fireball to be learned for free -bwr
    if (avail_levels < 1 && !player_has_spell(SPELL_DELAYED_FIREBALL))
    {
        mpr("You can't memorise any more spells yet.");
        return (-1);
    }
    else if (inv_count() < 1)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        return (-1);
    }

    mprf("You can memorise %d more level%s of spells.",
         avail_levels, (avail_levels > 1) ? "s" : "" );

    book = prompt_invent_item("Memorise from which spellbook?", MT_INVLIST,
                              OSEL_MEMORISE );
    if (prompt_failed(book))
        return (-1);

    if (you.inv[book].base_type != OBJ_BOOKS
        || you.inv[book].sub_type == BOOK_MANUAL)
    {
        mpr("That isn't a spellbook!");
        return (-1);
    }

    if (you.inv[book].sub_type == BOOK_DESTRUCTION)
    {
        tome_of_power( book );
        return (-1);
    }

    return (book);
}

// Returns false if the player cannot read/memorise from the book,
// and true otherwise. -- bwr
bool player_can_read_spellbook( const item_def &book )
{
    if (book.base_type != OBJ_BOOKS)
        return (true);

    if (book.props.exists( SPELL_LIST_KEY ))
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

void mark_had_book(const item_def &book)
{
    ASSERT(book.base_type == OBJ_BOOKS);

    if (book.book_number() == BOOK_MANUAL
        || book.book_number() == BOOK_DESTRUCTION)
    {
        return;
    }

    for (int i = 0; i < SPELLBOOK_SIZE; i++)
    {
        spell_type stype = which_spell_in_book(book, i);
        if (stype == SPELL_NO_SPELL)
            continue;

        you.seen_spell[stype] = true;
    }

    if (!book.props.exists( SPELL_LIST_KEY ))
        mark_had_book(book.book_number());
}

void mark_had_book(int booktype)
{
    ASSERT(booktype >= 0 && booktype < NUM_BOOKS);

    if (booktype == BOOK_MANUAL || booktype == BOOK_DESTRUCTION)
    {
        return;
    }

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

    // Remember that this function is called from staff spells as well.
    const int keyin = spellbook_contents( book, action );

    if (book.base_type == OBJ_BOOKS)
        mark_had_book(book);

    redraw_screen();

    // Put special book effects in another function which can be called
    // from memorise as well.

    set_ident_flags( book, ISFLAG_KNOW_TYPE );

    return (keyin);
}

// Recoded to answer whether an UNDEAD_STATE is
// barred from a particular spell passed to the
// function. Note that the function can be expanded
// to prevent memorisation of certain spells by
// the living by setting up an US_ALIVE case returning
// a value of false for a set of spells ... might be
// an idea worth further consideration. - 12mar2000 {dlb}
bool undead_cannot_memorise(spell_type spell, char being)
{
    switch (being)
    {
    case US_HUNGRY_DEAD: // Ghouls
        switch (spell)
        {
        case SPELL_AIR_WALK:
        case SPELL_ALTER_SELF:
        case SPELL_BERSERKER_RAGE:
        case SPELL_BLADE_HANDS:
        case SPELL_BORGNJORS_REVIVIFICATION:
        case SPELL_CURE_POISON_II:
        case SPELL_DEATHS_DOOR:
        case SPELL_DRAGON_FORM:
        case SPELL_ICE_FORM:
        case SPELL_NECROMUTATION:
        case SPELL_RESIST_POISON:
        case SPELL_SPIDER_FORM:
        case SPELL_STATUE_FORM:
        case SPELL_STONESKIN:
        case SPELL_SYMBOL_OF_TORMENT:
        case SPELL_TAME_BEASTS:
            return (true);
        default:
            return (false);
        }
        break;

    case US_SEMI_UNDEAD: // Vampires
        switch (spell)
        {
        case SPELL_BORGNJORS_REVIVIFICATION:
        case SPELL_DEATHS_DOOR:
        case SPELL_NECROMUTATION:
            return (true);
        default:
            // In addition, the above US_HUNGRY_DEAD spells are not castable
            // when satiated or worse.
            return (false);
        }
        break;

    case US_UNDEAD: // Mummies
        switch (spell)
        {
        case SPELL_AIR_WALK:
        case SPELL_ALTER_SELF:
        case SPELL_BERSERKER_RAGE:
        case SPELL_BLADE_HANDS:
        case SPELL_BORGNJORS_REVIVIFICATION:
        case SPELL_CURE_POISON_II:
        case SPELL_DEATHS_DOOR:
        case SPELL_DRAGON_FORM:
        case SPELL_ICE_FORM:
        case SPELL_INTOXICATE:
        case SPELL_NECROMUTATION:
        case SPELL_PASSWALL:
        case SPELL_REGENERATION:
        case SPELL_RESIST_POISON:
        case SPELL_SPIDER_FORM:
        case SPELL_STATUE_FORM:
        case SPELL_STONESKIN:
        case SPELL_SUMMON_HORRIBLE_THINGS:
        case SPELL_SYMBOL_OF_TORMENT:
        case SPELL_TAME_BEASTS:
            return (true);
        default:
            return (false);
        }
        break;
    }

    return (false);
}

bool player_can_memorise(const item_def &book)
{
    if (book.base_type != OBJ_BOOKS || book.sub_type == BOOK_MANUAL
        || book.sub_type == BOOK_DESTRUCTION)
    {
        return (false);
    }

    if (!player_spell_levels())
        return (false);

    for (int j = 0; j < SPELLBOOK_SIZE; j++)
    {
        const spell_type stype = which_spell_in_book(book, j);

        if (stype == SPELL_NO_SPELL)
            continue;

        // Easiest spell already too difficult?
        if (spell_difficulty(stype) > you.experience_level
            || player_spell_levels() < spell_levels_required(stype))
        {
            return (false);
        }

        bool knows_spell = false;
        for (int i = 0; i < 25 && !knows_spell; i++)
            knows_spell = (you.spells[i] == stype);

        // You don't already know this spell.
        if (!knows_spell)
            return (true);
    }
    return (false);
}

bool learn_spell(int book)
{
    int chance = 0;
    int levels_needed = 0;
    int index;

    int i;
    int j = 0;

    for (i = SK_SPELLCASTING; i <= SK_POISON_MAGIC; i++)
        if (you.skills[i])
            j++;

    if (j == 0)
    {
        mpr("You can't use spell magic! I'm afraid it's scrolls only for now.");
        return (false);
    }

    if (you.confused())
    {
        mpr("You are too confused!");
        return (false);
    }

    if (you.duration[DUR_BERSERKER])
    {
        canned_msg(MSG_TOO_BERSERK);
        return (false);
    }

    if (book < 0)
        book = _which_spellbook();

    if (book < 0) // still -1?
        return (false);

    int spell = read_book( you.inv[book], RBOOK_MEMORISE );
    clrscr();

    mesclr(true);
    redraw_screen();

    if ( !isalpha(spell) )
    {
        canned_msg( MSG_HUH );
        return (false);
    }

    index = letter_to_index( spell );

    if (index >= SPELLBOOK_SIZE
        || !is_valid_spell_in_book( you.inv[book], index ))
    {
        canned_msg( MSG_HUH );
        return (false);
    }

    spell_type specspell = which_spell_in_book(you.inv[book], index);

    if (specspell == SPELL_NO_SPELL)
    {
        canned_msg( MSG_HUH );
        return (false);
    }

    // You can always memorise selective amnesia:
    if (you.spell_no >= 21 && specspell != SPELL_SELECTIVE_AMNESIA)
    {
        mpr("Your head is already too full of spells!");
        return (false);
    }

    if ((you.is_undead || you.species == SP_DEMONSPAWN)
        && spell_typematch(specspell, SPTYP_HOLY))
    {
        mpr("You can't use this type of magic!");
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

    if (player_mutation_level(MUT_BLURRY_VISION) > 0
        && x_chance_in_y(player_mutation_level(MUT_BLURRY_VISION), 4))
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
            MiscastEffect( &you, MISC_KNOWN_MISCAST, SPTYP_NECROMANCY,
                           8, random2avg(88, 3),
                           "reading the Necronomicon" );
        }
        else if (you.inv[ book ].sub_type == BOOK_DEMONOLOGY)
        {
            mpr("This book does not appreciate being disturbed by one of your ineptitude!");
            MiscastEffect( &you, MISC_KNOWN_MISCAST, SPTYP_SUMMONING,
                           7, random2avg(88, 3),
                           "reading the book of Demonology" );
        }
        else if (you.inv[ book ].sub_type == BOOK_ANNIHILATIONS)
        {
            mpr("This book does not appreciate being disturbed by one of your ineptitude!");
            MiscastEffect( &you, MISC_KNOWN_MISCAST, SPTYP_CONJURATION,
                           8, random2avg(88, 3),
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
    while (nspel < SPELLBOOK_SIZE && is_valid_spell_in_book(item, nspel))
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

    bool need_id = false;
    if (!item_type_known(istaff))
    {
        set_ident_type( OBJ_STAVES, istaff.sub_type, ID_KNOWN_TYPE );
        set_ident_flags( istaff, ISFLAG_KNOW_TYPE );
        need_id = true;
    }
    if (!item_ident( istaff, ISFLAG_KNOW_PLUSES))
    {
        set_ident_flags( istaff, ISFLAG_KNOW_PLUSES );
        need_id = true;
    }
    if (need_id)
    {
        mprf(MSGCH_EQUIPMENT, "%s", istaff.name(DESC_INVENTORY_EQUIP).c_str());
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

    if ((idx >= SPELLBOOK_SIZE) || !is_valid_spell_in_book(istaff, idx))
    {
        canned_msg(MSG_HUH);
        return -1;
    }

    const spell_type spell = which_spell_in_book( istaff, idx );
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

        food = calc_hunger(food);
    }

    if (food && (you.hunger_state == HS_STARVING || you.hunger <= food))
    {
        mpr("You don't have the energy to cast that spell.");
        crawl_state.zero_turns_taken();
        return (-1);
    }

    if (istaff.plus < mana)
    {
        mpr("The rod doesn't have enough magic points.");
        crawl_state.zero_turns_taken();
        // Don't lose a turn for trying to evoke without enough MP - that's
        // needlessly cruel for an honest error.
        return (-1);
    }

    const int flags = get_spell_flags(spell);

    // Labyrinths block divinations.
    if (you.level_type == LEVEL_LABYRINTH
        && testbits(flags, SPFLAG_MAPPING))
    {
        mpr("Something interferes with your magic!");
    }
    // All checks passed, we can cast the spell.
    else if (your_spells(spell, powc, false) == SPRET_ABORT)
    {
        crawl_state.zero_turns_taken();
        return (-1);
    }

    make_hungry(food, true, true);
    istaff.plus -= mana;
    you.wield_change = true;
    you.turn_is_over = true;

    return (roll_dice( 1, 1 + spell_difficulty(spell) / 2 ));
}

static bool _compare_spells(spell_type a, spell_type b)
{
    if (a == SPELL_NO_SPELL && b == SPELL_NO_SPELL)
        return (false);
    else if (a != SPELL_NO_SPELL && b == SPELL_NO_SPELL)
        return (true);
    else if (a == SPELL_NO_SPELL && b != SPELL_NO_SPELL)
        return (false);

    int level_a = spell_difficulty(a);
    int level_b = spell_difficulty(b);

    if (level_a != level_b)
        return (level_a < level_b);

    unsigned int schools_a = get_spell_disciplines(a);
    unsigned int schools_b = get_spell_disciplines(b);

    if (schools_a != schools_b && schools_a != 0 && schools_b != 0)
    {
        const char* a_type = NULL;
        const char* b_type = NULL;

        // Find lowest/earliest school for each spell.
        for (int i = 0; i <= SPTYP_LAST_EXPONENT; i++)
        {
            int mask = 1 << i;
            if (a_type == NULL && (schools_a & mask))
                a_type = spelltype_name(mask);
            if (b_type == NULL && (schools_b & mask))
                b_type = spelltype_name(mask);
        }
        ASSERT(a_type != NULL && b_type != NULL);
        return (strcmp(a_type, b_type));
    }

    return (strcmp(spell_title(a), spell_title(b)));
}

static bool _is_memorized(spell_type spell)
{
    for (int i = 0; i < 25; i++)
    {
        if (you.spells[i] == spell)
            return (true);
    }

    return (false);
}

static void _get_spell_list(std::vector<spell_type> &spell_list, int level,
                            unsigned int disc1, unsigned int disc2,
                            god_type god, bool avoid_uncastable,
                            int &god_discard, int &uncastable_discard)
{
    for (int i = 0; i < NUM_SPELLS; i++)
    {
        const spell_type spell = (spell_type) i;

        if (!is_valid_spell(spell))
            continue;

        // Only use spells available in books you might find laying about
        // the dungeon.
        if (spell_rarity(spell) == -1)
            continue;

        const unsigned int disciplines = get_spell_disciplines(spell);
        if (level != -1)
        {
            if (spell_difficulty(spell) != level)
                continue;
        }
        else if (!((disciplines & disc1) || (disciplines & disc2))
                 || disciplines_conflict(disc1, disciplines)
                 || disciplines_conflict(disc2, disciplines))
        {
            continue;
        }

        // Only wizards gets spells still under development.
        const unsigned int flags = get_spell_flags(spell);
        if (flags & SPFLAG_DEVEL)
        {
#ifdef WIZARD
            if (!you.wizard)
                continue;
#else
            continue;
#endif
        }

        if (avoid_uncastable && undead_cannot_memorise(spell, you.is_undead))
        {
            uncastable_discard++;
            continue;
        }

        if (god_dislikes_spell_type(spell, god))
        {
            god_discard++;
            continue;
        }

        // Passed all tests.
        spell_list.push_back(spell);
    }
}

static void _get_spell_list(std::vector<spell_type> &spell_list,
                            unsigned int disc1, unsigned int disc2,
                            god_type god, bool avoid_uncastable,
                            int &god_discard, int &uncastable_discard)
{
    _get_spell_list(spell_list, -1, disc1, disc2,
                    god, avoid_uncastable, god_discard, uncastable_discard);
}

static void _get_spell_list(std::vector<spell_type> &spell_list, int level,
                            god_type god, bool avoid_uncastable,
                            int &god_discard, int &uncastable_discard)
{
    _get_spell_list(spell_list, level, SPTYP_NONE, SPTYP_NONE,
                    god, avoid_uncastable, god_discard, uncastable_discard);
}


bool make_book_level_randart(item_def &book, int level,
                             int num_spells)
{
    ASSERT(book.base_type == OBJ_BOOKS);
    ASSERT(book.book_number()    != BOOK_MANUAL
           && book.book_number() != BOOK_DESTRUCTION);
    ASSERT(is_random_artefact(book));
    ASSERT(!book.props.exists(SPELL_LIST_KEY));

    god_type god;
    (void) origin_is_god_gift(book, &god);

    const bool avoid_uncastable = (god != GOD_NO_GOD && god != GOD_XOM)
                                  || origin_is_acquirement(book);
    const bool track_levels_had = origin_is_acquirement(book)
                                  || god == GOD_SIF_MUNA;

    if (level == -1)
    {
        int max_level =
            avoid_uncastable ? std::min(9, you.get_experience_level())
                             : 9;

        level = random_range(1, max_level);

        // Give a book of a level not seen before, preferably one with
        // spells of a low enough level for the player to cast, or the
        // lowest aviable level if all levels which the player can cast
        // have already been given.
        if (track_levels_had)
        {
            unsigned int seen_levels = you.attribute[ATTR_RND_LVL_BOOKS];
            std::vector<int> vec;
            for (int i = 1; i <= 9 && (vec.empty() || i <= max_level); i++)
            {
                if (!(seen_levels & (1 << i)))
                    vec.push_back(i);
            }
            if (vec.size() > 0)
                level = vec[random2(vec.size())];
        }
    }
    ASSERT(level > 0 && level <= 9);

    if (num_spells == -1)
        num_spells = SPELLBOOK_SIZE;
    ASSERT(num_spells > 0 && num_spells <= SPELLBOOK_SIZE);

    int god_discard        = 0;
    int uncastable_discard = 0;

    std::vector<spell_type> spell_list;
    _get_spell_list(spell_list, level, god, avoid_uncastable,
                    god_discard, uncastable_discard);

    if (spell_list.empty())
    {
        char buf[80];

        if (god_discard > 0 && uncastable_discard == 0)
            sprintf(buf, "%s disliked all level %d spells",
                    god_name(god).c_str(), level);
        else if (god_discard == 0 && uncastable_discard > 0)
            sprintf(buf, "No level %d spells can be cast by you", level);
        else if (god_discard > 0 && uncastable_discard > 0)
            sprintf(buf, "All level %d spells are either disliked by %s "
                         "or cannot be cast by you.",
                    level, god_name(god).c_str());
        else
            sprintf(buf, "No level %d spells?!?!?!", level);

        mprf(MSGCH_ERROR, "Could not create fixed level randart spellbook: %s",
             buf);

        return (false);
    }
    random_shuffle(spell_list.begin(), spell_list.end());

    if (num_spells > (int) spell_list.size())
    {
        // Some gods (Elyvilon) dislike a lot of the higher level spells,
        // so try a lower level.
        if (god != GOD_NO_GOD && god != GOD_XOM)
            return make_book_level_randart(book, level - 1, num_spells);

        num_spells = spell_list.size();
#if DEBUG || DEBUG_DIAGNOSTICS
        // Not many level 8 or 9 spells
        if (level < 8)
        {
            mprf(MSGCH_WARN, "More spells requested for fixed level (%d) "
                             "randart spellbook than there are valid spells.",
                 level);
            mprf(MSGCH_WARN, "Discarded %d spells due to being uncastable and "
                             "%d spells due to being disliked by %s.",
                 uncastable_discard, god_discard, god_name(god).c_str());
        }
#endif
    }

    std::vector<bool> spell_used(spell_list.size(), false);
    std::vector<bool> avoid_memorised(spell_list.size(), avoid_uncastable);

    spell_type chosen_spells[SPELLBOOK_SIZE];
    for (int i = 0; i < SPELLBOOK_SIZE; i++)
    {
        chosen_spells[i] = SPELL_NO_SPELL;
    }

    int book_pos = 0;
    while (book_pos < num_spells)
    {
        int spell_pos = random2(spell_list.size());

        if (spell_used[spell_pos])
            continue;

        spell_type spell = spell_list[spell_pos];
        ASSERT(spell != SPELL_NO_SPELL);

        if (avoid_memorised[spell_pos] && _is_memorized(spell))
        {
           // Only once.
           avoid_memorised[spell_pos] = false;
           continue;
        }

        spell_used[spell_pos]     = true;
        chosen_spells[book_pos++] = spell;
    }
    std::sort(chosen_spells, chosen_spells + SPELLBOOK_SIZE,
              _compare_spells);
    ASSERT(chosen_spells[0] != SPELL_NO_SPELL);

    CrawlHashTable &props = book.props;
    props[SPELL_LIST_KEY].new_vector(SV_LONG).resize(SPELLBOOK_SIZE);

    CrawlVector &spell_vec = props[SPELL_LIST_KEY];
    spell_vec.set_max_size(SPELLBOOK_SIZE);

    for (int i = 0; i < SPELLBOOK_SIZE; i++)
        spell_vec[i] = (long) chosen_spells[i];

    if (track_levels_had)
        you.attribute[ATTR_RND_LVL_BOOKS] |= (1 << level);

    return (true);
}

bool make_book_theme_randart(item_def &book, int disc1, int disc2,
                             int num_spells, int max_levels)
{
    ASSERT(book.base_type == OBJ_BOOKS);
    ASSERT(book.book_number()    != BOOK_MANUAL
           && book.book_number() != BOOK_DESTRUCTION);
    ASSERT(is_random_artefact(book));
    ASSERT(!book.props.exists(SPELL_LIST_KEY));

    god_type god;
    (void) origin_is_god_gift(book, &god);

    const bool avoid_uncastable = (god != GOD_NO_GOD && god != GOD_XOM)
                                  || origin_is_acquirement(book);

    if (num_spells == -1)
        num_spells = SPELLBOOK_SIZE;
    ASSERT(num_spells > 0 && num_spells <= SPELLBOOK_SIZE);

    if (max_levels == -1)
        max_levels = INT_MAX;
    ASSERT(max_levels > 0);

    if (disc1 == 0 && disc2 == 0)
    {
        // Eliminate disciplines that the god disapproves of or from which
        // all spells are discarded.
        std::vector<int> ok_discs;
        for (int i = 0; i < SPTYP_LAST_EXPONENT; i++)
        {
            int disc = 1 << i;
            if (god_dislikes_spell_discipline(disc, god))
                continue;
            int junk1 = 0, junk2 = 0;

            std::vector<spell_type> spell_list;
            _get_spell_list(spell_list, disc, disc, god, avoid_uncastable,
                            junk1, junk2);

            if(spell_list.empty())
                continue;

            ok_discs.push_back(i);
        }

        ASSERT( !ok_discs.empty() );
        if (ok_discs.empty())
        {
            mpr("No valid disciplines with which to make a themed ranadart "
                "spellbook.", MSGCH_ERROR);
            return (false);
        }

        do
        {
            disc1 = 1 << ok_discs[random2(ok_discs.size())];
            disc2 = 1 << ok_discs[random2(ok_discs.size())];
        } while(disciplines_conflict(disc1, disc2));
    }
    else if (disc2 == 0)
        disc2 = disc1;

    ASSERT(disc1 < (1 << (SPTYP_LAST_EXPONENT + 1)));
    ASSERT(disc2 < (1 << (SPTYP_LAST_EXPONENT + 1)));
    ASSERT(count_bits(disc1) == 1 && count_bits(disc2) == 1);

    int god_discard        = 0;
    int uncastable_discard = 0;

    std::vector<spell_type> spell_list;
    _get_spell_list(spell_list, disc1, disc2, god, avoid_uncastable,
                    god_discard, uncastable_discard);

    if (num_spells > (int) spell_list.size())
        num_spells = spell_list.size();

    std::vector<bool> spell_used(spell_list.size(), false);

    spell_type chosen_spells[SPELLBOOK_SIZE];
    for (int i = 0; i < SPELLBOOK_SIZE; i++)
    {
        chosen_spells[i] = SPELL_NO_SPELL;
    }

    int book_pos = 0;
    while (book_pos < num_spells && max_levels > 0)
    {
        int spell_pos = random2(spell_list.size());

        if (spell_used[spell_pos])
            continue;

        spell_type spell = spell_list[spell_pos];
        ASSERT(spell != SPELL_NO_SPELL);

        int spell_level = spell_difficulty(spell);
        if (spell_level > max_levels)
            continue;

        spell_used[spell_pos]     = true;
        chosen_spells[book_pos++] = spell;
        max_levels               -= spell_level;
    }
    std::sort(chosen_spells, chosen_spells + SPELLBOOK_SIZE,
              _compare_spells);
    ASSERT(chosen_spells[0] != SPELL_NO_SPELL);

    CrawlHashTable &props = book.props;
    props[SPELL_LIST_KEY].new_vector(SV_LONG).resize(SPELLBOOK_SIZE);

    CrawlVector &spell_vec = props[SPELL_LIST_KEY];
    spell_vec.set_max_size(SPELLBOOK_SIZE);

    for (int i = 0; i < SPELLBOOK_SIZE; i++)
        spell_vec[i] = (long) chosen_spells[i];

    std::string name;

    if (god != GOD_NO_GOD)
    {
        name  = '"';
        name += god_name(god, false) + "'s book";
    }

    name += " of ";
    name += spelltype_long_name(disc1);

    if (disc1 != disc2)
    {
        name += " and ";
        name += spelltype_long_name(disc2);
    }

    if (god != GOD_NO_GOD)
        name += '"';

    set_randart_name(book, name);

    return (true);
}

bool book_has_title(const item_def &book)
{
    ASSERT(book.base_type == OBJ_BOOKS);

    if (!is_artefact(book))
        return (false);

    return (get_artefact_name(book)[0] == '"');
}
