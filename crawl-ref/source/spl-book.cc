/*
 *  File:       spl-book.cc
 *  Summary:    Spellbook/Staff contents array and management functions
 *  Written by: Josh Fishman
 */

#include "AppHdr.h"

#include "spl-book.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <iomanip>

#include "artefact.h"
#include "externs.h"
#include "species.h"
#include "cio.h"
#include "database.h"
#include "debug.h"
#include "delay.h"
#include "food.h"
#include "format.h"
#include "goditem.h"
#include "invent.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "kills.h"
#include "macro.h"
#include "message.h"
#include "player.h"
#include "religion.h"
#include "godconduct.h"
#include "spl-cast.h"
#include "spl-mis.h"
#include "spl-util.h"
#include "state.h"
#include "stuff.h"
#include "transform.h"
#include "colour.h"

#define SPELL_LIST_KEY "spell_list"

#define RANDART_BOOK_TYPE_KEY  "randart_book_type"
#define RANDART_BOOK_LEVEL_KEY "randart_book_level"

#define RANDART_BOOK_TYPE_LEVEL "level"
#define RANDART_BOOK_TYPE_THEME "theme"

#define NUMBER_SPELLBOOKS sizeof(spellbook_template_array)/(sizeof(spell_type) * SPELLBOOK_SIZE)

static spell_type spellbook_template_array[][SPELLBOOK_SIZE] =
{
    // Minor Magic I (fire)
    {SPELL_MAGIC_DART,
     SPELL_SUMMON_SMALL_MAMMALS,
     SPELL_THROW_FLAME,
     SPELL_BLINK,
     SPELL_SLOW,
     SPELL_MEPHITIC_CLOUD,
     SPELL_CONJURE_FLAME,
     SPELL_NO_SPELL,
     },

    // Minor Magic II (ice)
    {SPELL_MAGIC_DART,
     SPELL_THROW_FROST,
     SPELL_BLINK,
     SPELL_STICKS_TO_SNAKES,
     SPELL_SLOW,
     SPELL_MEPHITIC_CLOUD,
     SPELL_OZOCUBUS_ARMOUR,
     SPELL_NO_SPELL,
     },

    // Minor Magic III (summ)
    {SPELL_MAGIC_DART,
     SPELL_SUMMON_SMALL_MAMMALS,
     SPELL_BLINK,
     SPELL_REPEL_MISSILES,
     SPELL_SLOW,
     SPELL_CALL_CANINE_FAMILIAR,
     SPELL_MEPHITIC_CLOUD,
     SPELL_NO_SPELL,
     },

    // Book of Conjurations I - Fire and Earth
    {SPELL_MAGIC_DART,
     SPELL_THROW_FLAME,
     SPELL_STONE_ARROW,
     SPELL_CONJURE_FLAME,
     SPELL_BOLT_OF_MAGMA,
     SPELL_FIREBALL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Book of Conjurations II - Air and Ice
    {SPELL_MAGIC_DART,
     SPELL_THROW_FROST,
     SPELL_MEPHITIC_CLOUD,
     SPELL_DISCHARGE,
     SPELL_BOLT_OF_COLD,
     SPELL_FREEZING_CLOUD,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Book of Flames
    {SPELL_FLAME_TONGUE,
     SPELL_THROW_FLAME,
     SPELL_CONJURE_FLAME,
     SPELL_STICKY_FLAME,
     SPELL_BOLT_OF_FIRE,
     SPELL_FIREBALL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Book of Frost
    {SPELL_FREEZE,
     SPELL_THROW_FROST,
     SPELL_OZOCUBUS_ARMOUR,
     SPELL_THROW_ICICLE,
     SPELL_SUMMON_ICE_BEAST,
     SPELL_FREEZING_CLOUD,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Book of Summonings
    {SPELL_ABJURATION,
     SPELL_RECALL,
     SPELL_SUMMON_UGLY_THING,
     SPELL_SHADOW_CREATURES,
     SPELL_HAUNT,
     SPELL_SUMMON_HORRIBLE_THINGS,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Book of Fire
    {SPELL_EVAPORATE,
     SPELL_FIRE_BRAND,
     SPELL_SUMMON_ELEMENTAL,
     SPELL_BOLT_OF_MAGMA,
     SPELL_IGNITE_POISON,
     SPELL_DELAYED_FIREBALL,
     SPELL_RING_OF_FLAMES,
     SPELL_NO_SPELL,
     },

    // Book of Ice
    {SPELL_FREEZING_AURA,
     SPELL_HIBERNATION,
     SPELL_CONDENSATION_SHIELD,
     SPELL_OZOCUBUS_REFRIGERATION,
     SPELL_BOLT_OF_COLD,
     SPELL_SIMULACRUM,
     SPELL_ENGLACIATION,
     SPELL_NO_SPELL,
     },


    // Book of Spatial Translocations
    {SPELL_APPORTATION,
     SPELL_PORTAL_PROJECTILE,
     SPELL_BLINK,
     SPELL_RECALL,
     SPELL_TELEPORT_OTHER,
     SPELL_CONTROL_TELEPORT,
     SPELL_TELEPORT_SELF,
     SPELL_NO_SPELL,
     },

    // Book of Enchantments (fourth one)
    {SPELL_LEVITATION,
     SPELL_SELECTIVE_AMNESIA,
     SPELL_SEE_INVISIBLE,
     SPELL_CAUSE_FEAR,
     SPELL_EXTENSION,
     SPELL_DEFLECT_MISSILES,
     SPELL_HASTE,
     SPELL_NO_SPELL,
     },

    // Young Poisoner's Handbook
    {SPELL_STING,
     SPELL_CURE_POISON,
     SPELL_POISON_WEAPON,
     SPELL_MEPHITIC_CLOUD,
     SPELL_VENOM_BOLT,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Book of the Tempests
    {SPELL_DISCHARGE,
     SPELL_LIGHTNING_BOLT,
     SPELL_FIREBALL,
     SPELL_SHATTER,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Book of Death
    {SPELL_CORPSE_ROT,
     SPELL_LETHAL_INFUSION,
     SPELL_BONE_SHARDS,
     SPELL_AGONY,
     SPELL_BOLT_OF_DRAINING,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Book of Hinderance
    {SPELL_CONFUSING_TOUCH,
     SPELL_SLOW,
     SPELL_CONFUSE,
     SPELL_PETRIFY,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Book of Changes
    {SPELL_FULSOME_DISTILLATION,
     SPELL_STICKS_TO_SNAKES,
     SPELL_EVAPORATE,
     SPELL_SPIDER_FORM,
     SPELL_ICE_FORM,
     SPELL_DIG,
     SPELL_BLADE_HANDS,
     SPELL_NO_SPELL,
     },

    // Book of Transfigurations
    {SPELL_SANDBLAST,
     SPELL_POLYMORPH_OTHER,
     SPELL_STATUE_FORM,
     SPELL_ALTER_SELF,
     SPELL_DRAGON_FORM,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Book of War Chants
    {SPELL_FIRE_BRAND,
     SPELL_FREEZING_AURA,
     SPELL_REPEL_MISSILES,
     SPELL_BERSERKER_RAGE,
     SPELL_REGENERATION,
     SPELL_HASTE,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Book of Clouds
    {SPELL_EVAPORATE,
     SPELL_MEPHITIC_CLOUD,
     SPELL_CONJURE_FLAME,
     SPELL_POISONOUS_CLOUD,
     SPELL_FREEZING_CLOUD,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Book of Necromancy
    {SPELL_PAIN,
     SPELL_ANIMATE_SKELETON,
     SPELL_VAMPIRIC_DRAINING,
     SPELL_REGENERATION,
     SPELL_DISPEL_UNDEAD,
     SPELL_ANIMATE_DEAD,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Book of Callings
    {SPELL_SUMMON_SMALL_MAMMALS,
     SPELL_STICKS_TO_SNAKES,
     SPELL_CALL_IMP,
     SPELL_CALL_CANINE_FAMILIAR,
     SPELL_SUMMON_SCORPIONS,
     SPELL_SUMMON_ICE_BEAST,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Book of Charms
    {SPELL_CORONA,
     SPELL_REPEL_MISSILES,
     SPELL_HIBERNATION,
     SPELL_CONFUSE,
     SPELL_ENSLAVEMENT,
     SPELL_SILENCE,
     SPELL_INVISIBILITY,
     SPELL_NO_SPELL,
     },

    // Book of Air
    {SPELL_SHOCK,
     SPELL_SWIFTNESS,
     SPELL_REPEL_MISSILES,
     SPELL_LEVITATION,
     SPELL_MEPHITIC_CLOUD,
     SPELL_DISCHARGE,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Book of the Sky
    {SPELL_SUMMON_ELEMENTAL,
     SPELL_INSULATION,
     SPELL_AIRSTRIKE,
     SPELL_FLY,
     SPELL_SILENCE,
     SPELL_LIGHTNING_BOLT,
     SPELL_DEFLECT_MISSILES,
     SPELL_CONJURE_BALL_LIGHTNING,
     },


    // Book of the Warp
    {SPELL_BANISHMENT,
     SPELL_PHASE_SHIFT,
     SPELL_WARP_BRAND,
     SPELL_DISPERSAL,
     SPELL_PORTAL,
     SPELL_CONTROLLED_BLINK,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Book of Envenomations
    {SPELL_SPIDER_FORM,
     SPELL_POISON_WEAPON,
     SPELL_SUMMON_SCORPIONS,
     SPELL_RESIST_POISON,
     SPELL_OLGREBS_TOXIC_RADIANCE,
     SPELL_POISONOUS_CLOUD,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Book of Unlife
    {SPELL_SUBLIMATION_OF_BLOOD,
     SPELL_ANIMATE_DEAD,
     SPELL_TWISTED_RESURRECTION,
     SPELL_BORGNJORS_REVIVIFICATION,
     SPELL_EXCRUCIATING_WOUNDS,
     SPELL_SIMULACRUM,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Book of Control
    {SPELL_CONTROL_TELEPORT,
     SPELL_ENSLAVEMENT,
     SPELL_TAME_BEASTS,
     SPELL_MASS_CONFUSION,
     SPELL_CONTROL_UNDEAD,
     SPELL_ENGLACIATION,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Book of Morphology
    {SPELL_FRAGMENTATION,
     SPELL_POLYMORPH_OTHER,
     SPELL_CIGOTUVIS_DEGENERATION,
     SPELL_ALTER_SELF,
     SPELL_SHATTER,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Book of Tukima
    {SPELL_SURE_BLADE,
     SPELL_TUKIMAS_VORPAL_BLADE,
     SPELL_TUKIMAS_DANCE,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Book of Geomancy
    {SPELL_SANDBLAST,
     SPELL_STONESKIN,
     SPELL_PASSWALL,
     SPELL_STONE_ARROW,
     SPELL_SUMMON_ELEMENTAL,
     SPELL_FRAGMENTATION,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Book of Earth
    {SPELL_MAXWELLS_SILVER_HAMMER,
     SPELL_DIG,
     SPELL_STATUE_FORM,
     SPELL_IRON_SHOT,
     SPELL_SHATTER,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL
     },

    // Book of Wizardry
    {SPELL_SELECTIVE_AMNESIA,
     SPELL_SUMMON_ELEMENTAL,
     SPELL_TELEPORT_SELF,
     SPELL_FIREBALL,
     SPELL_HASTE,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Book of Power
    {SPELL_ANIMATE_DEAD,
     SPELL_ISKENDERUNS_MYSTIC_BLAST,
     SPELL_TELEPORT_OTHER,
     SPELL_VENOM_BOLT,
     SPELL_IRON_SHOT,
     SPELL_INVISIBILITY,
     SPELL_MASS_CONFUSION,
     SPELL_POISONOUS_CLOUD,
     },

    // Book of Cantrips
    {SPELL_CONFUSING_TOUCH,
     SPELL_ANIMATE_SKELETON,
     SPELL_SUMMON_SMALL_MAMMALS,
     SPELL_APPORTATION,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Book of Party Tricks
    {SPELL_SUMMON_BUTTERFLIES,
     SPELL_APPORTATION,
     SPELL_PROJECTED_NOISE,
     SPELL_BLINK,
     SPELL_LEVITATION,
     SPELL_INTOXICATE,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Book of Beasts
    {SPELL_SUMMON_SMALL_MAMMALS,
     SPELL_STICKS_TO_SNAKES,
     SPELL_CALL_CANINE_FAMILIAR,
     SPELL_TAME_BEASTS,
     SPELL_DRAGON_FORM,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Book of Stalking
    {SPELL_STING,
     SPELL_SURE_BLADE,
     SPELL_PROJECTED_NOISE,
     SPELL_POISON_WEAPON,
     SPELL_MEPHITIC_CLOUD,
     SPELL_PETRIFY,
     SPELL_INVISIBILITY,
     SPELL_NO_SPELL,
     },

    // Book of Brands
    {SPELL_CORONA,
     SPELL_SWIFTNESS,
     SPELL_FIRE_BRAND,
     SPELL_FREEZING_AURA,
     SPELL_POISON_WEAPON,
     SPELL_CAUSE_FEAR,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Tome of the Dragon
    {SPELL_FLY,
     SPELL_CAUSE_FEAR,
     SPELL_BOLT_OF_FIRE,
     SPELL_DRAGON_FORM,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

     // Book of Burglary
    {SPELL_APPORTATION,
     SPELL_SWIFTNESS,
     SPELL_PASSWALL,
     SPELL_CONTROL_TELEPORT,
     SPELL_FRAGMENTATION,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

     // Book of Dreams
    {SPELL_HIBERNATION,
     SPELL_INTOXICATE,
     SPELL_FLY,
     SPELL_PHASE_SHIFT,
     SPELL_SHADOW_CREATURES,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

     // Book of Chemistry
    {SPELL_FULSOME_DISTILLATION,
     SPELL_LETHAL_INFUSION,
     SPELL_SUBLIMATION_OF_BLOOD,
     SPELL_EVAPORATE,
     SPELL_CONDENSATION_SHIELD,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Book of Annihilations - Vehumet special
    {SPELL_POISON_ARROW,
     SPELL_IOOD,
     SPELL_CHAIN_LIGHTNING,
     SPELL_LEHUDIBS_CRYSTAL_SPEAR,
     SPELL_ICE_STORM,
     SPELL_FIRE_STORM,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Book of Demonology - Vehumet special
    {SPELL_CALL_IMP,
     SPELL_ABJURATION,
     SPELL_RECALL,
     SPELL_SUMMON_DEMON,
     SPELL_DEMONIC_HORDE,
     SPELL_SUMMON_GREATER_DEMON,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Necronomicon - Kikubaaqudgha special
    {SPELL_SYMBOL_OF_TORMENT,
     SPELL_CONTROL_UNDEAD,
     SPELL_HAUNT,
     SPELL_DEATHS_DOOR,
     SPELL_NECROMUTATION,
     SPELL_DEATH_CHANNEL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Randart Spellbook (by level)
    {SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Randart Spellbook (by theme)
    {SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Book of Card Effects
    {SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // manuals of all kinds
    {SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Tome of Destruction
    {SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Rods - start at NUM_BOOKS.

    // Rod of smiting
    {SPELL_SMITING,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Rod of summoning
    {SPELL_ABJURATION,
     SPELL_RECALL,
     SPELL_SUMMON_ELEMENTAL,
     SPELL_SUMMON_SWARM,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Rod of destruction (fire)
    {SPELL_THROW_FLAME,
     SPELL_BOLT_OF_FIRE,
     SPELL_FIREBALL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Rod of destruction (ice)
    {SPELL_THROW_FROST,
     SPELL_THROW_ICICLE,
     SPELL_FREEZING_CLOUD,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Rod of destruction (lightning, iron, fireball)
    {SPELL_LIGHTNING_BOLT,
     SPELL_IRON_SHOT,
     SPELL_FIREBALL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Rod of destruction (inaccuracy, magma, cold)
    {SPELL_BOLT_OF_INACCURACY,
     SPELL_BOLT_OF_MAGMA,
     SPELL_BOLT_OF_COLD,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Rod of warding
    {SPELL_ABJURATION,
     SPELL_CONDENSATION_SHIELD,
     SPELL_CAUSE_FEAR,
     SPELL_DEFLECT_MISSILES,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Rod of discovery
    {SPELL_DETECT_SECRET_DOORS,
     SPELL_DETECT_TRAPS,
     SPELL_DETECT_ITEMS,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Rod of demonology
    {SPELL_CALL_IMP,
     SPELL_ABJURATION,
     SPELL_RECALL,
     SPELL_SUMMON_DEMON,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Rod of striking
    {SPELL_STRIKING,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Rod of venom
    {SPELL_CURE_POISON,
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
    ASSERT( sbook_type < static_cast<int>(NUMBER_SPELLBOOKS) );
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

    out.cprintf( "\n\n Spells                             Type                      Level\n" );

    for (j = 0; j < SPELLBOOK_SIZE; j++)
    {
        spell_type stype = which_spell_in_book(book, j);
        if (stype == SPELL_NO_SPELL)
            continue;

        out.cprintf(" ");

        const int level_diff = spell_difficulty( stype );
        const int levels_req = spell_levels_required( stype );

        int colour = DARKGREY;
        if (action == RBOOK_USE_STAFF)
        {
            if (book.base_type == OBJ_BOOKS ?
                   (you.experience_level >= level_diff
                    && you.magic_points >= level_diff
                    && player_can_memorise_from_spellbook(book))
                : book.plus >= level_diff * ROD_CHARGE_MULT)
            {
                colour = spell_highlight_by_utility(stype, COL_UNKNOWN, false, false, true);
            }
            else
                colour = COL_USELESS;
        }
        else
        {
            if (you_cannot_memorise(stype)
                || you.experience_level < level_diff
                || spell_levels < levels_req
                || !spell_skills
                || book.base_type == OBJ_BOOKS
                   && !player_can_memorise_from_spellbook(book))
            {
                colour = COL_USELESS;
            }
            else
                colour = spell_highlight_by_utility(stype, COL_UNKNOWN,
                                                    false, true);
        }

        out.textcolor( colour );

        char strng[2];
        strng[0] = index_to_letter(spelcount);
        strng[1] = 0;

        out.cprintf(strng);
        out.cprintf(" - ");

        out.cprintf("%-29s", spell_title(stype));

        std::string schools;
        if (action == RBOOK_USE_STAFF)
            schools = "Evocations";
        else
        {
            bool first = true;
            for (i = 0; i <= SPTYP_LAST_EXPONENT; i++)
            {
                if (spell_typematch(stype, 1 << i))
                {
                    if (!first)
                        schools += "/";
                    schools += spelltype_long_name(1 << i);
                    first = false;
                }
            }
        }
        out.cprintf("%-30s", schools.c_str());

        char sval[3];
        itoa(level_diff, sval, 10);
        out.cprintf(sval);
        out.cprintf("\n");
        spelcount++;
    }

    out.textcolor(LIGHTGREY);
    out.cprintf("\n");

    switch (action)
    {
    case RBOOK_USE_STAFF:
        out.cprintf( "Select a spell to cast.\n" );
        break;

    case RBOOK_READ_SPELL:
        if (book.base_type == OBJ_BOOKS && in_inventory(book)
            && item_type_known(book)
            && player_can_memorise_from_spellbook(book))
        {
            out.cprintf( "Select a spell to read its description or to "
                         "memorise it.\n" );
        }
        else
            out.cprintf( "Select a spell to read its description.\n" );
        break;

    default:
        break;
    }

    if (fs)
        *fs = out;

    int keyn = 0;
    if (update_screen && !crawl_state.is_replaying_keys())
    {
        cursor_control coff(false);
        clrscr();

        out.display();
    }

    if (update_screen)
        keyn = tolower(getchm(KMC_MENU));

    return (keyn);     // try to figure out that for which this is used {dlb}
}

//jmf: was in shopping.cc
// Rarity 100 is reserved for unused books.
int book_rarity(unsigned char which_book)
{
    switch (which_book)
    {
    case BOOK_MINOR_MAGIC_I:
    case BOOK_MINOR_MAGIC_II:
    case BOOK_MINOR_MAGIC_III:
    case BOOK_HINDERANCE:
    case BOOK_CANTRIPS: //jmf: added 04jan2000
        return 1;

    case BOOK_CHANGES:
    case BOOK_CHARMS:
        return 2;

    case BOOK_CONJURATIONS_I:
    case BOOK_CONJURATIONS_II:
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
    case BOOK_BRANDS:
        return 5;

    case BOOK_CLOUDS:
    case BOOK_POWER:
        return 6;

    case BOOK_ENCHANTMENTS:
    case BOOK_PARTY_TRICKS:     //jmf: added 04jan2000
        return 7;

    case BOOK_TRANSFIGURATIONS:
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
    case BOOK_BURGLARY:
    case BOOK_CHEMISTRY:
    case BOOK_DREAMS:
        return 12;

    case BOOK_ENVENOMATIONS:
    case BOOK_WARP:
    case BOOK_DRAGON:
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

    default:
        return 1;
    }
}

static unsigned char _lowest_rarity[NUM_SPELLS];

void init_spell_rarities()
{
    for (int i = 0; i < NUM_SPELLS; ++i)
        _lowest_rarity[i] = 255;

    for (int i = 0; i < NUM_FIXED_BOOKS; ++i)
    {
        // Manuals and books of destruction are not even part of this loop.
        if (i >= MIN_GOD_ONLY_BOOK && i <= MAX_GOD_ONLY_BOOK)
            continue;

        for (int j = 0; j < SPELLBOOK_SIZE; ++j)
        {
            spell_type spell = which_spell_in_book(i, j);
            if (spell == SPELL_NO_SPELL)
                continue;

#ifdef DEBUG
            unsigned int flags = get_spell_flags(spell);

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

            const int rarity = book_rarity(i);
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

// Returns false if the player cannot memorise from the book,
// and true otherwise. -- bwr
bool player_can_memorise_from_spellbook( const item_def &book )
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

    if (book.sub_type == BOOK_RANDART_LEVEL)
    {
        god_type god;
        int      level = book.plus;
        ASSERT(level > 0 && level <= 9);

        if (origin_is_acquirement(book)
            || origin_is_god_gift(book, &god) && god == GOD_SIF_MUNA)
        {
            you.attribute[ATTR_RND_LVL_BOOKS] |= (1 << level);
        }
    }

    if (!book.props.exists( SPELL_LIST_KEY ))
        mark_had_book(book.book_number());
}

void mark_had_book(int booktype)
{
    ASSERT(booktype >= 0 && booktype <= MAX_FIXED_BOOK);

    you.had_book[booktype] = true;

    if (booktype == BOOK_MINOR_MAGIC_I
        || booktype == BOOK_MINOR_MAGIC_II
        || booktype == BOOK_MINOR_MAGIC_III)
    {
        you.had_book[BOOK_MINOR_MAGIC_I]   = true;
        you.had_book[BOOK_MINOR_MAGIC_II]  = true;
        you.had_book[BOOK_MINOR_MAGIC_III] = true;
    }
    else if (booktype == BOOK_CONJURATIONS_I
             || booktype == BOOK_CONJURATIONS_II)
    {
        you.had_book[BOOK_CONJURATIONS_I]  = true;
        you.had_book[BOOK_CONJURATIONS_II] = true;
    }
}

void inscribe_book_highlevel(item_def &book)
{
    if (!item_type_known(book)
        && book.inscription.find("highlevel") == std::string::npos)
    {
        add_inscription(book, "highlevel");
    }
}

int read_book( item_def &book, read_book_action_type action )
{
    if (book.base_type == OBJ_BOOKS && !item_type_known(book)
        && !player_can_memorise_from_spellbook(book))
    {
        mpr( "This book is beyond your current level of understanding." );
        more();

        inscribe_book_highlevel(book);
        return (0);
    }

    // Remember that this function is called from staff spells as well.
    const int keyin = spellbook_contents( book, action );

    if (book.base_type == OBJ_BOOKS)
        mark_had_book(book);

    if (!crawl_state.is_replaying_keys())
        redraw_screen();

    // Put special book effects in another function which can be called
    // from memorise as well.

    set_ident_flags( book, ISFLAG_KNOW_TYPE );
    set_ident_flags( book, ISFLAG_IDENT_MASK);

    return (keyin);
}

bool you_cannot_memorise(spell_type spell)
{
    bool temp;
    return you_cannot_memorise(spell, temp);
}

// undead is set to true if being undead prevents us from memorising the spell.
bool you_cannot_memorise(spell_type spell, bool &undead)
{
    bool rc = false;

    switch (you.is_undead)
    {
    case US_HUNGRY_DEAD: // Ghouls
        switch (spell)
        {
        case SPELL_ALTER_SELF:
        case SPELL_BERSERKER_RAGE:
        case SPELL_BLADE_HANDS:
        case SPELL_BORGNJORS_REVIVIFICATION:
        case SPELL_CURE_POISON:
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
            rc = true;
            break;
        default:
            break;
        }
        break;

    case US_SEMI_UNDEAD: // Vampires
        switch (spell)
        {
        case SPELL_BORGNJORS_REVIVIFICATION:
        case SPELL_DEATHS_DOOR:
        case SPELL_NECROMUTATION:
            // In addition, the above US_HUNGRY_DEAD spells are not castable
            // when satiated or worse.
            rc = true;
            break;
        default:
            break;
        }
        break;

    case US_UNDEAD: // Mummies
        switch (spell)
        {
        case SPELL_ALTER_SELF:
        case SPELL_BERSERKER_RAGE:
        case SPELL_BLADE_HANDS:
        case SPELL_BORGNJORS_REVIVIFICATION:
        case SPELL_CURE_POISON:
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
        case SPELL_SYMBOL_OF_TORMENT:
        case SPELL_TAME_BEASTS:
            rc = true;
            break;
        default:
            break;
        }
        break;

    case US_ALIVE:
        break;
    }

    // If rc has been set to true before now, that was because we
    // are (possibly temporarily) undead.
    if (rc == true)
        undead = true;

    if (you.species == SP_DEEP_DWARF && spell == SPELL_REGENERATION)
        rc = true;

    return (rc);
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

typedef std::vector<spell_type>   spell_list;
typedef std::map<spell_type, int> spells_to_books;

static bool _get_mem_list(spell_list &mem_spells,
                          spells_to_books &book_hash,
                          unsigned int &num_unreadable,
                          unsigned int &num_race,
                          bool just_check = false,
                          spell_type current_spell = SPELL_NO_SPELL)
{
    bool          book_errors    = false;
    unsigned int  num_books      = 0;
                  num_unreadable = 0;

    // Collect the list of all spells in all available spellbooks.
    for (int i = 0; i < ENDOFPACK; i++)
    {
        item_def& book(you.inv[i]);

        if (book.base_type != OBJ_BOOKS || book.sub_type == BOOK_DESTRUCTION
            || book.sub_type == BOOK_MANUAL)
        {
            continue;
        }

        num_books++;

        if (!player_can_memorise_from_spellbook(book))
        {
            inscribe_book_highlevel(book);
            num_unreadable++;
            continue;
        }

        mark_had_book(book);
        set_ident_flags(book, ISFLAG_KNOW_TYPE);
        set_ident_flags(book, ISFLAG_IDENT_MASK);

        int spells_in_book = 0;
        for (int j = 0; j < SPELLBOOK_SIZE; j++)
        {
            if (!is_valid_spell_in_book(book, j))
                continue;

            const spell_type spell = which_spell_in_book(book, j);

            spells_in_book++;

            // XXX: If same spell is in two different dangerous spellbooks,
            // how to decide which one to use?
            spells_to_books::iterator it = book_hash.find(spell);
            if (it == book_hash.end() || is_dangerous_spellbook(it->second))
                book_hash[spell] = book.sub_type;
        }

        if (spells_in_book == 0)
        {
            mprf(MSGCH_ERROR, "Spellbook \"%s\" contains no spells! Please "
                 "file a bug report.", book.name(DESC_PLAIN).c_str());
            book_errors = true;
        }
    }

    if (book_errors)
        more();

    if (num_books == 0)
    {
        if (!just_check)
            mpr("You aren't carrying any spellbooks.", MSGCH_PROMPT);
        return (false);
    }
    else if (num_unreadable == num_books)
    {
        if (!just_check)
        {
            mpr("All of the spellbooks you're carrying are beyond your "
                "current level of comprehension.", MSGCH_PROMPT);
        }
        return (false);
    }
    else if (book_hash.size() == 0)
    {
        if (!just_check)
        {
            mpr("None of the spellbooks you are carrying contain any spells.",
                MSGCH_PROMPT);
        }
        return (false);
    }

    unsigned int num_known      = 0;
                 num_race       = 0;
    unsigned int num_low_xl     = 0;
    unsigned int num_low_levels = 0;
    unsigned int num_memable    = 0;

    bool amnesia = false;

    for (spells_to_books::iterator i = book_hash.begin();
         i != book_hash.end(); ++i)
    {
        const spell_type spell = i->first;

        if (spell == current_spell || you.has_spell(spell))
            num_known++;
        else if (you_cannot_memorise(spell))
            num_race++;
        else
        {
            mem_spells.push_back(spell);

            int avail_slots = player_spell_levels();
            if (current_spell != SPELL_NO_SPELL)
                avail_slots -= spell_levels_required(current_spell);

            if (spell_difficulty(spell) > you.experience_level)
                num_low_xl++;
            else if (avail_slots < spell_levels_required(spell))
                num_low_levels++;
            else
            {
                if (spell == SPELL_SELECTIVE_AMNESIA)
                    amnesia = true;
                num_memable++;
            }
        }
    }

    // You can always memorise selective amnesia.
    if (num_memable > 0 && you.spell_no >= 21)
    {
        if (amnesia)
        {
            mem_spells.clear();
            mem_spells.push_back(SPELL_SELECTIVE_AMNESIA);
            return (true);
        }

        if (!just_check)
            mpr("Your head is already too full of spells!");
        return (false);
    }

    if (num_memable)
        return (true);

    // Return true even if there are only spells we can't memorise _yet_.
    if (just_check)
        return (num_low_levels > 0 || num_low_xl > 0);

    unsigned int total = num_known + num_race + num_low_xl + num_low_levels;

    if (num_known == total)
        mpr("You already know all available spells.", MSGCH_PROMPT);
    else if (num_race == total || (num_known + num_race) == total)
    {
        const bool lichform = (you.attribute[ATTR_TRANSFORMATION] == TRAN_LICH);
        const std::string species = "a " + species_name(you.species, 0);
        mprf(MSGCH_PROMPT,
             "You cannot memorise any of the available spells because you "
             "are %s %s.", lichform ? "in Lich form"
                                    : lowercase_string(species).c_str());
    }
    else if (num_low_levels > 0 || num_low_xl > 0)
    {
        // Just because we can't memorise them doesn't mean we don't want to
        // see what we have available. See FR #235. {due}
        return (true);
    }
    else
    {
        mpr("You can't memorise any new spells for an unknown reason; "
            "please file a bug report.", MSGCH_PROMPT);
    }

    if (num_unreadable)
    {
        mprf(MSGCH_PROMPT, "Additionally, %u of your spellbooks are beyond "
             "your current level of understanding, and thus none of the "
             "spells in them are available to you.", num_unreadable);
    }

    return (false);
}

// If current_spell is a valid spell, returns whether you'll be able to
// memorise any further spells once this one is committed to memory.
bool has_spells_to_memorise(bool silent, int current_spell)
{
    spell_list      mem_spells;
    spells_to_books book_hash;
    unsigned int    num_unreadable;
    unsigned int    num_race;

    return _get_mem_list(mem_spells, book_hash, num_unreadable, num_race,
                         silent, (spell_type) current_spell);
}

static bool _sort_mem_spells(spell_type a, spell_type b)
{
    if (spell_fail(a) != spell_fail(b))
        return (spell_fail(a) < spell_fail(b));
    if (spell_difficulty(a) != spell_difficulty(b))
        return (spell_difficulty(a) < spell_difficulty(b));

    return (stricmp(spell_title(a), spell_title(b)) < 0);
}

std::vector<spell_type> get_mem_spell_list(std::vector<int> &books)
{
    std::vector<spell_type> spells;

    spell_list      mem_spells;
    spells_to_books book_hash;
    unsigned int    num_unreadable;
    unsigned int    num_race;

    if (!_get_mem_list(mem_spells, book_hash, num_unreadable, num_race))
        return (spells);

    std::sort(mem_spells.begin(), mem_spells.end(), _sort_mem_spells);

    for (unsigned int i = 0; i < mem_spells.size(); i++)
    {
        spell_type spell = mem_spells[i];
        spells.push_back(spell);

        spells_to_books::iterator it = book_hash.find(spell);
        books.push_back(it->second);
    }

    return (spells);
}

static spell_type _choose_mem_spell(spell_list &spells,
                                    spells_to_books &book_hash,
                                    unsigned long num_unreadable,
                                    unsigned long num_race)
{
    std::sort(spells.begin(), spells.end(), _sort_mem_spells);

#ifdef USE_TILE
    const bool text_only = false;
#else
    const bool text_only = true;
#endif

    Menu spell_menu(MF_SINGLESELECT | MF_ANYPRINTABLE
                    | MF_ALWAYS_SHOW_MORE | MF_ALLOW_FORMATTING,
                    "", text_only);
#ifdef USE_TILE
    {
        // [enne] - Hack.  Make title an item so that it's aligned.
        MenuEntry* me =
            new MenuEntry("    Spells                         Type          "
                          "                Success  Level",
                MEL_ITEM);
        me->colour = BLUE;
        spell_menu.add_entry(me);
    }
#else
    spell_menu.set_title(
        new MenuEntry("     Spells                        Type          "
                      "                Success  Level",
            MEL_TITLE));
#endif

    spell_menu.set_highlighter(NULL);
    spell_menu.set_tag("spell");

    std::string more_str = make_stringf("<lightgreen>%d spell level%s left"
                                        "<lightgreen>",
                                        player_spell_levels(),
                                        (player_spell_levels() > 1
                                         || player_spell_levels() == 0) ? "s" : "");

    if (num_unreadable > 0)
    {
        more_str += make_stringf(", <lightmagenta>%u overly difficult "
                                 "spellbook%s</lightmagenta>",
                                 num_unreadable,
                                 num_unreadable > 1 ? "s" : "");
    }

    if (num_race > 0)
    {
        more_str += make_stringf(", <lightred>%u spell%s unmemorisable"
                                 "</lightred>",
                                 num_race,
                                 num_race > 1 ? "s" : "");
    }

    spell_menu.set_more(formatted_string::parse_string(more_str));

    // Don't make a menu so tall that we recycle hotkeys on the same page.
    if (spells.size() > 52
        && (spell_menu.maxpagesize() > 52 || spell_menu.maxpagesize() == 0))
    {
        spell_menu.set_maxpagesize(52);
    }


    for (unsigned int i = 0; i < spells.size(); i++)
    {
        const spell_type spell = spells[i];

        spells_to_books::iterator it = book_hash.find(spell);
        const bool dangerous = is_dangerous_spellbook(it->second);

        std::ostringstream desc;

        int colour = LIGHTGRAY;
        // Grey out spells for which you lack experience or spell levels.
        if (spell_difficulty(spell) > you.experience_level
            || player_spell_levels() < spell_levels_required(spell))
        {
            colour = DARKGRAY;
        }
        else
            colour = spell_highlight_by_utility(spell);

        // Highlight dangerous books magenta, but don't bother if they are
        // already highlighted as forbidden by the player's god.
        if (dangerous && colour != COL_FORBIDDEN)
            colour = MAGENTA;

        desc << "<" << colour_to_str(colour) << ">";

        desc << std::left;
        desc << std::setw(30) << spell_title(spell);
        desc << spell_schools_string(spell);

        int so_far = desc.str().length() - ( name_length_by_colour(colour)+2);
        if (so_far < 60)
            desc << std::string(60 - so_far, ' ');

        desc << std::setw(12) << failure_rate_to_string(spell_fail(spell))
             << spell_difficulty(spell);

        desc << "</" << colour_to_str(colour) << ">";

        MenuEntry* me = new MenuEntry(desc.str(), MEL_ITEM, 1,
                                      index_to_letter(i % 52));

#ifdef USE_TILE
        me->add_tile(tile_def(tileidx_spell(spell), TEX_GUI));
#endif

        me->data = &spells[i];
        spell_menu.add_entry(me);
    }

    while (true)
    {
        std::vector<MenuEntry*> sel = spell_menu.show();

        if (!crawl_state.doing_prev_cmd_again)
            redraw_screen();

        if (sel.empty())
            return (SPELL_NO_SPELL);

        ASSERT(sel.size() == 1);

        const spell_type spell = *static_cast<spell_type*>(sel[0]->data);
        ASSERT(is_valid_spell(spell));

        return (spell);
    }
}

bool can_learn_spell(bool silent)
{
    if (player_in_bat_form())
    {
        if (!silent)
            canned_msg(MSG_PRESENT_FORM);
        return (false);
    }

    if (you.stat_zero[STAT_INT])
    {
        if (!silent)
            mpr("Your brain is not functional enough to learn spells.");
        return (false);
    }

    int i;
    int j = 0;

    for (i = SK_SPELLCASTING; i <= SK_POISON_MAGIC; i++)
        if (you.skills[i])
            j++;

    if (j == 0)
    {
        if (!silent)
        {
            mpr("You can't use spell magic! I'm afraid it's scrolls only "
                "for now.");
        }
        return (false);
    }

    if (you.confused())
    {
        if (!silent)
            mpr("You are too confused!");
        return (false);
    }

    if (you.berserk())
    {
        if (!silent)
            canned_msg(MSG_TOO_BERSERK);
        return (false);
    }

    return (true);
}

bool learn_spell()
{
    if (!can_learn_spell())
        return (false);

    spell_list      mem_spells;
    spells_to_books book_hash;

    unsigned int num_unreadable, num_race;

    if (!_get_mem_list(mem_spells, book_hash, num_unreadable, num_race))
        return (false);

    spell_type specspell = _choose_mem_spell(mem_spells, book_hash,
                                             num_unreadable, num_race);

    if (specspell == SPELL_NO_SPELL)
    {
        canned_msg( MSG_OK );
        return (false);
    }

    spells_to_books::iterator it = book_hash.find(specspell);

    return learn_spell(specspell, it->second, true);
}

// Returns a string about why an undead character can't memorise a spell.
std::string desc_cannot_memorise_reason(bool undead)
{
    if (undead)
        ASSERT(you.is_undead);

    const bool lichform = (undead
                           && you.attribute[ATTR_TRANSFORMATION] == TRAN_LICH);

    std::string desc = "You cannot ";
    if (lichform)
        desc += "currently ";
    desc += "memorise or cast this spell because you are ";

    if (lichform)
        desc += "in Lich form";
    else
        desc += "a " + lowercase_string(species_name(you.species, 0));

    desc += ".";

    return (desc);
}

static bool _learn_spell_checks(spell_type specspell)
{
    if (!can_learn_spell())
        return (false);

    if (already_learning_spell((int) specspell))
        return (false);

    bool undead = false;
    if (you_cannot_memorise(specspell, undead))
    {
        mprf(desc_cannot_memorise_reason(undead).c_str());
        return (false);
    }

    if (you.has_spell(specspell))
    {
        mpr("You already know that spell!");
        return (false);
    }

    if (you.spell_no >= 21 && specspell != SPELL_SELECTIVE_AMNESIA)
    {
        mpr("Your head is already too full of spells!");
        return (false);
    }

    if (you.experience_level < spell_difficulty(specspell))
    {
        mpr("You're too inexperienced to learn that spell!");
        return (false);
    }

    if (player_spell_levels() < spell_levels_required(specspell))
    {
        mpr("You can't memorise that many levels of magic yet!");
        return (false);
    }
    return (true);
}

bool learn_spell(spell_type specspell, int book, bool is_safest_book)
{
    if (!_learn_spell_checks(specspell))
        return (false);

    int chance = spell_fail(specspell);

    if (chance > 0 && is_dangerous_spellbook(book))
    {
        std::string prompt;

        if (is_safest_book)
            prompt = "The only spellbook you have which contains that spell ";
        else
            prompt = "The spellbook you are reading from ";

        item_def fakebook;
        fakebook.base_type = OBJ_BOOKS;
        fakebook.sub_type  = book;
        fakebook.quantity  = 1;
        fakebook.flags    |= ISFLAG_IDENT_MASK;

        prompt += make_stringf("is %s, a dangerous spellbook which will "
                               "strike back at you if your memorisation "
                               "attempt fails. Attempt to memorise anyway?",
                               fakebook.name(DESC_NOCAP_THE).c_str());

        // Deactivate choice from tile inventory.
        mouse_control mc(MOUSE_MODE_MORE);
        if (!yesno(prompt.c_str(), false, 'n'))
        {
            canned_msg( MSG_OK );
            return (false);
        }
    }

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

    snprintf(info, INFO_SIZE,
             "Memorise %s, consuming %d spell level%s and leaving %d?",
             spell_title(specspell), spell_levels_required(specspell),
             spell_levels_required(specspell) != 1 ? "s" : "",
             player_spell_levels() - spell_levels_required(specspell));

    // Deactivate choice from tile inventory.
    mouse_control mc(MOUSE_MODE_MORE);
    if (!yesno(info, true, 'n', false))
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

        if (book == BOOK_NECRONOMICON)
        {
            mpr("The pages of the Necronomicon glow with a dark malevolence...");
            MiscastEffect( &you, MISC_KNOWN_MISCAST, SPTYP_NECROMANCY,
                           8, random2avg(88, 3),
                           "reading the Necronomicon" );
        }
        else if (book == BOOK_DEMONOLOGY)
        {
            mpr("This book does not appreciate being disturbed by one of your ineptitude!");
            MiscastEffect( &you, MISC_KNOWN_MISCAST, SPTYP_SUMMONING,
                           7, random2avg(88, 3),
                           "reading the book of Demonology" );
        }
        else if (book == BOOK_ANNIHILATIONS)
        {
            mpr("This book does not appreciate being disturbed by one of your ineptitude!");
            MiscastEffect( &you, MISC_KNOWN_MISCAST, SPTYP_CONJURATION,
                           8, random2avg(88, 3),
                           "reading the book of Annihilations" );
        }

#ifdef WIZARD
        if (!you.wizard)
            return (false);
        else if (!yesno("Memorise anyway?", true, 'n'))
            return (false);
#else
        return (false);
#endif
    }

    start_delay( DELAY_MEMORISE, spell_difficulty( specspell ), specspell );
    you.turn_is_over = true;

    did_god_conduct( DID_SPELL_CASTING, 2 + random2(5) );

    return (true);
}

int count_staff_spells(const item_def &item, bool need_id)
{
    if (item.base_type != OBJ_STAVES)
        return (-1);

    if (need_id && !item_type_known(item))
        return (0);

    const int type = item.book_number();
    if (!item_is_rod(item) || type == -1)
        return (0);

    int nspel = 0;
    while (nspel < SPELLBOOK_SIZE && is_valid_spell_in_book(item, nspel))
        ++nspel;

    return (nspel);
}

int staff_spell( int staff )
{
    item_def& istaff(you.inv[staff]);
    // Spell staves are mostly for the benefit of non-spellcasters, so we're
    // not going to involve INT or Spellcasting skills for power. -- bwr
    int powc = (5 + you.skills[SK_EVOCATIONS]
                 + roll_dice( 2, you.skills[SK_EVOCATIONS] ));

    if (!item_is_rod(istaff))
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return (-1);
    }

    // ID code got moved to item_use::wield_effects. {due}

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

    if ( !isaalpha(keyin) )
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

    if (food && (you.hunger_state == HS_STARVING || you.hunger <= food)
        && !you.is_undead)
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
                a_type = spelltype_long_name(mask);
            if (b_type == NULL && (schools_b & mask))
                b_type = spelltype_long_name(mask);
        }
        ASSERT(a_type != NULL && b_type != NULL);
        return (strcmp(a_type, b_type) < 0);
    }

    return (strcmp(spell_title(a), spell_title(b)) < 0);
}

bool is_memorised(spell_type spell)
{
    for (int i = 0; i < 25; i++)
        if (you.spells[i] == spell)
            return (true);

    return (false);
}

static void _get_spell_list(std::vector<spell_type> &spell_list, int level,
                            unsigned int disc1, unsigned int disc2,
                            god_type god, bool avoid_uncastable,
                            int &god_discard, int &uncastable_discard,
                            bool avoid_known = false)
{
    // For randarts handed out by Sif Muna, spells contained in the
    // Vehumet/Kiku specials are fair game.
    // We store them in an extra vector that (once sorted) can later
    // be checked for each spell with a rarity -1 (i.e. not normally
    // appearing randomly).
    std::vector<spell_type> special_spells;
    if (god == GOD_SIF_MUNA)
    {
        for (int i = MIN_GOD_ONLY_BOOK; i <= MAX_GOD_ONLY_BOOK; ++i)
            for (int j = 0; j < SPELLBOOK_SIZE; ++j)
            {
                spell_type spell = which_spell_in_book(i, j);
                if (spell == SPELL_NO_SPELL)
                    continue;

                if (spell_rarity(spell) != -1)
                    continue;

                special_spells.push_back(spell);
            }

        std::sort(special_spells.begin(), special_spells.end());
    }

    int specnum = 0;
    for (int i = 0; i < NUM_SPELLS; ++i)
    {
        const spell_type spell = (spell_type) i;

        if (!is_valid_spell(spell))
            continue;

        // Only use spells available in books you might find lying about
        // the dungeon.
        if (spell_rarity(spell) == -1)
        {
            bool skip_spell = true;
            while ((unsigned int) specnum < special_spells.size()
                   && spell == special_spells[specnum])
            {
                specnum++;
                skip_spell = false;
            }

            if (skip_spell)
                continue;
        }

        if (avoid_known && you.seen_spell[spell])
            continue;

        if (level != -1)
        {
            // fixed level randart: only include spells of the given level
            if (spell_difficulty(spell) != level)
                continue;
        }
        else
        {
            // themed randart: only include spells of the given disciplines
            const unsigned int disciplines = get_spell_disciplines(spell);
            if ((!(disciplines & disc1) && !(disciplines & disc2))
                 || disciplines_conflict(disc1, disciplines)
                 || disciplines_conflict(disc2, disciplines))
            {
                continue;
            }
        }

        if (avoid_uncastable && you_cannot_memorise(spell))
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
                            int &god_discard, int &uncastable_discard,
                            bool avoid_known = false)
{
    _get_spell_list(spell_list, -1, disc1, disc2,
                    god, avoid_uncastable, god_discard, uncastable_discard,
                    avoid_known);
}

static void _get_spell_list(std::vector<spell_type> &spell_list, int level,
                            god_type god, bool avoid_uncastable,
                            int &god_discard, int &uncastable_discard,
                            bool avoid_known = false)
{
    _get_spell_list(spell_list, level, SPTYP_NONE, SPTYP_NONE,
                    god, avoid_uncastable, god_discard, uncastable_discard,
                    avoid_known);
}


bool make_book_level_randart(item_def &book, int level, int num_spells,
                             std::string owner)
{
    ASSERT(book.base_type == OBJ_BOOKS);

    god_type god;
    (void) origin_is_god_gift(book, &god);

    const bool completely_random =
        god == GOD_XOM || (god == GOD_NO_GOD && !origin_is_acquirement(book));

    if (!is_random_artefact(book))
    {
        if (!owner.empty())
            book.props["owner"].get_string() = owner;

        // Stuff parameters into book.plus and book.plus2, then call
        // make_item_randart(), which will call us back.
        if (level == -1)
        {
            int max_level =
                (completely_random ? 9
                                   : std::min(9, you.get_experience_level()));

            level = random_range(1, max_level);
        }
        ASSERT(level > 0 && level <= 9);

        if (num_spells == -1)
            num_spells = SPELLBOOK_SIZE;
        ASSERT(num_spells > 0 && num_spells <= SPELLBOOK_SIZE);

        book.plus  = level;
        book.plus2 = num_spells;

        book.sub_type = BOOK_RANDART_LEVEL;
        return (make_item_randart(book));
    }

    // Being called from make_item_randart().
    ASSERT(book.sub_type == BOOK_RANDART_LEVEL);

    // Re-read owner, if applicable.
    if (owner.empty() && book.props.exists("owner"))
        owner = book.props["owner"].get_string();

    ASSERT(level > 0 && level <= 9);

    ASSERT(num_spells > 0 && num_spells <= SPELLBOOK_SIZE);

    int god_discard        = 0;
    int uncastable_discard = 0;

    std::vector<spell_type> spell_list;
    _get_spell_list(spell_list, level, god, !completely_random,
                    god_discard, uncastable_discard);

    if (spell_list.empty())
    {
        char buf[80];

        if (god_discard > 0 && uncastable_discard == 0)
        {
            snprintf(buf, sizeof(buf), "%s disliked all level %d spells",
                    god_name(god).c_str(), level);
        }
        else if (god_discard == 0 && uncastable_discard > 0)
            sprintf(buf, "No level %d spells can be cast by you", level);
        else if (god_discard > 0 && uncastable_discard > 0)
        {
            snprintf(buf, sizeof(buf),
                         "All level %d spells are either disliked by %s "
                         "or cannot be cast by you.",
                    level, god_name(god).c_str());
        }
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
#if defined(DEBUG) || defined(DEBUG_DIAGNOSTICS)
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
    std::vector<bool> avoid_memorised(spell_list.size(), !completely_random);
    std::vector<bool> avoid_seen(spell_list.size(), !completely_random);

    spell_type chosen_spells[SPELLBOOK_SIZE];
    for (int i = 0; i < SPELLBOOK_SIZE; i++)
        chosen_spells[i] = SPELL_NO_SPELL;

    int book_pos = 0;
    while (book_pos < num_spells)
    {
        int spell_pos = random2(spell_list.size());

        if (spell_used[spell_pos])
            continue;

        spell_type spell = spell_list[spell_pos];
        ASSERT(spell != SPELL_NO_SPELL);

        if (avoid_memorised[spell_pos] && is_memorised(spell))
        {
            // Only once.
            avoid_memorised[spell_pos] = false;
            continue;
        }

        if (avoid_seen[spell_pos] && you.seen_spell[spell] && coinflip())
        {
            // Only once.
            avoid_seen[spell_pos] = false;
            continue;
        }

        spell_used[spell_pos]     = true;
        chosen_spells[book_pos++] = spell;
    }
    std::sort(chosen_spells, chosen_spells + SPELLBOOK_SIZE,
              _compare_spells);
    ASSERT(chosen_spells[0] != SPELL_NO_SPELL);

    CrawlHashTable &props = book.props;
    props.erase(SPELL_LIST_KEY);
    props[SPELL_LIST_KEY].new_vector(SV_LONG).resize(SPELLBOOK_SIZE);

    CrawlVector &spell_vec = props[SPELL_LIST_KEY].get_vector();
    spell_vec.set_max_size(SPELLBOOK_SIZE);

    for (int i = 0; i < SPELLBOOK_SIZE; i++)
        spell_vec[i] = (long) chosen_spells[i];

    bool has_owner = true;
    std::string name = "";
    if (!owner.empty())
        name = owner;
    else if (god != GOD_NO_GOD)
        name = god_name(god, false);
    else if (one_chance_in(30))
        name = god_name(GOD_SIF_MUNA, false);
    else if (one_chance_in(3))
        name = make_name(random_int(), false);
    else
        has_owner = false;

    if (has_owner)
        name = apostrophise(name) + " ";

    // None of these books need a definite article prepended.
    book.props["is_named"].get_bool() = true;

    std::string bookname;
    if (god == GOD_XOM && coinflip())
    {
        bookname = getRandNameString("book_noun") + " of "
                   + getRandNameString("Xom_book_title");
    }
    else
    {
        std::string lookup;
        if (level == 1)
            lookup = "starting";
        else if (level <= 3 || level == 4 && coinflip())
            lookup = "easy";
        else if (level <= 6)
            lookup = "moderate";
        else
            lookup = "difficult";

        lookup += " level book";

        // First try for names respecting the book's previous owner/author
        // (if one exists), then check for general difficulty.
        if (has_owner)
            bookname = getRandNameString(lookup + " owner");

        if (!has_owner || bookname.empty())
            bookname = getRandNameString(lookup);

        bookname = uppercase_first(bookname);
        if (has_owner)
        {
            if (bookname.substr(0, 4) == "The ")
                bookname = bookname.substr(4);
            else if (bookname.substr(0, 2) == "A ")
                bookname = bookname.substr(2);
            else if (bookname.substr(0, 3) == "An ")
                bookname = bookname.substr(3);
        }

        if (bookname.find("@level@", 0) != std::string::npos)
        {
            std::string number;
            switch (level)
            {
            case 1: number = "One"; break;
            case 2: number = "Two"; break;
            case 3: number = "Three"; break;
            case 4: number = "Four"; break;
            case 5: number = "Five"; break;
            case 6: number = "Six"; break;
            case 7: number = "Seven"; break;
            case 8: number = "Eight"; break;
            case 9: number = "Nine"; break;
            default:
                number = ""; break;
            }
            bookname = replace_all(bookname, "@level@", number);
        }
    }

    if (bookname.empty())
        bookname = getRandNameString("book");

    name += bookname;

    set_artefact_name(book, name);

    return (true);
}

static bool _get_weighted_discs(bool completely_random, god_type god,
                                int &disc1, int &disc2)
{
    // Eliminate disciplines that the god dislikes or from which all
    // spells are discarded.
    std::vector<int> ok_discs;
    std::vector<int> skills;
    std::vector<int> spellcount;
    for (int i = 0; i < SPTYP_LAST_EXPONENT; i++)
    {
        int disc = 1 << i;
        if (disc & SPTYP_DIVINATION)
            continue;

        if (god_dislikes_spell_discipline(disc, god))
            continue;

        int junk1 = 0, junk2 = 0;
        std::vector<spell_type> spell_list;
        _get_spell_list(spell_list, disc, disc, god, !completely_random,
                        junk1, junk2, !completely_random);

        if (spell_list.empty())
            continue;

        ok_discs.push_back(disc);
        skills.push_back(spell_type2skill(disc));
        spellcount.push_back(spell_list.size());
    }

    int num_discs = ok_discs.size();

    if (num_discs == 0)
    {
#ifdef DEBUG
        mpr("No valid disciplines with which to make a themed randart "
            "spellbook.", MSGCH_ERROR);
#endif
        // Only happens if !completely_random and the player already knows
        // all available spells. We could simply re-allow all disciplines
        // but the player isn't going to get any new spells, anyway, so just
        // consider this acquirement failed. (jpeg)
        return (false);
    }

    int skill_weights[SPTYP_LAST_EXPONENT];

    memset(skill_weights, 0, SPTYP_LAST_EXPONENT * sizeof(int));

    if (!completely_random)
    {
        int total_skills = 0;
        for (int i = 0; i < num_discs; i++)
        {
            int skill  = skills[i];
            int weight = 2 * you.skills[skill] + 1;

            if (spellcount[i] < 3)
                weight *= spellcount[i]/3;

            skill_weights[i] = std::max(0, weight);
            total_skills += skill_weights[i];
        }

        if (total_skills == 0)
            completely_random = true;
    }

    if (completely_random)
    {
        for (int i = 0; i < num_discs; i++)
            skill_weights[i] = 1;
    }

    do
    {
        disc1 = ok_discs[choose_random_weighted(skill_weights,
                                                skill_weights + num_discs)];
        disc2 = ok_discs[choose_random_weighted(skill_weights,
                                                skill_weights + num_discs)];
    }
    while (disciplines_conflict(disc1, disc2));

    return (true);
}

static void _get_weighted_spells(bool completely_random, god_type god,
                                 int disc1, int disc2,
                                 int num_spells, int max_levels,
                                 const std::vector<spell_type> &spell_list,
                                 spell_type chosen_spells[])
{
    ASSERT(num_spells <= (int) spell_list.size());
    ASSERT(num_spells <= SPELLBOOK_SIZE && num_spells > 0);
    ASSERT(max_levels > 0);

    int spell_weights[NUM_SPELLS];
    memset(spell_weights, 0, NUM_SPELLS * sizeof(int));

    if (completely_random)
    {
        for (unsigned int i = 0; i < spell_list.size(); i++)
        {
            spell_type spl = spell_list[i];
            if (god == GOD_XOM)
                spell_weights[spl] = count_bits(get_spell_disciplines(spl));
            else
                spell_weights[spl] = 1;
        }
    }
    else
    {
        const int Spc = you.skills[SK_SPELLCASTING];
        for (unsigned int i = 0; i < spell_list.size(); i++)
        {
            spell_type spell = spell_list[i];
            unsigned int disciplines = get_spell_disciplines(spell);

            int d = 1;
            if ((disciplines & disc1) && (disciplines & disc2))
                d = 2;

            int c = 1;
            if (!you.seen_spell[spell])
                c = 4;
            else if (!is_memorised(spell))
                c = 2;

            int total_skill = 0;
            int num_skills  = 0;
            for (int j = 0; j < SPTYP_LAST_EXPONENT; j++)
            {
                int disc = 1 << j;

                if (disciplines & disc)
                {
                    total_skill += you.skills[spell_type2skill(disc)];
                    num_skills++;
                }
            }
            int w = 1;
            if (num_skills > 0)
                w = (2 + (total_skill / num_skills)) / 3;
            w = std::max(1, w);

            int l = 5 - abs(3 * spell_difficulty(spell) - Spc) / 7;

            int weight = d * c * w * l;

            ASSERT(weight > 0);
            spell_weights[spell] = weight;
        }
    }

    int book_pos    = 0;
    int spells_left = spell_list.size();
    while (book_pos < num_spells && max_levels > 0 && spells_left > 0)
    {
        spell_type spell =
            (spell_type) choose_random_weighted(spell_weights,
                                                spell_weights + NUM_SPELLS);
        ASSERT(is_valid_spell(spell));
        ASSERT(spell_weights[spell] > 0);

        int levels = spell_difficulty(spell);

        if (levels > max_levels)
        {
            spell_weights[spell] = 0;
            spells_left--;
            continue;
        }
        chosen_spells[book_pos++] = spell;
        spell_weights[spell]      = 0;
        max_levels               -= levels;
        spells_left--;
    }
    ASSERT(book_pos > 0 && max_levels >= 0);
}

static void _remove_nondiscipline_spells(spell_type chosen_spells[],
                                         int d1, int d2,
                                         spell_type exclude = SPELL_NO_SPELL)
{
    int replace = -1;
    for (int i = 0; i < SPELLBOOK_SIZE; i++)
    {
        if (chosen_spells[i] == SPELL_NO_SPELL)
            break;

        if (chosen_spells[i] == exclude)
            continue;

        // If a spell matches neither the first nor the second type
        // (that may be the same) remove it.
        if (!spell_typematch(chosen_spells[i], d1)
            && !spell_typematch(chosen_spells[i], d2))
        {
            chosen_spells[i] = SPELL_NO_SPELL;
            if (replace == -1)
                replace = i;
        }
        else if (replace != -1)
        {
            chosen_spells[replace] = chosen_spells[i];
            chosen_spells[i] = SPELL_NO_SPELL;
            replace++;
        }
    }
}

// Takes a book of any type, a spell discipline or two, the number of spells
// (up to 8), the total spell levels of all spells, a spell that absolutely
// has to be included, and the name of whomever the book should be named after.
// With all that information the book is turned into a random artefact
// containing random spells of the given disciplines (random if none set).
// NOTE: This function calls make_item_randart() which recursively calls
//       make_book_theme_randart() again but without all those parameters,
//       so they have to be stored in the book attributes so as to not
//       forget them.
bool make_book_theme_randart(item_def &book, int disc1, int disc2,
                             int num_spells, int max_levels,
                             spell_type incl_spell, std::string owner)
{
    ASSERT(book.base_type == OBJ_BOOKS);

    god_type god;
    (void) origin_is_god_gift(book, &god);

    const bool completely_random =
        god == GOD_XOM || (god == GOD_NO_GOD && !origin_is_acquirement(book));

    if (!is_random_artefact(book))
    {
        // Store spell and owner for later use.
        if (incl_spell != SPELL_NO_SPELL)
            book.props["spell"].get_long() = incl_spell;
        if (!owner.empty())
            book.props["owner"].get_string() = owner;

        // Stuff parameters into book.plus and book.plus2, then call
        // make_item_randart(), which will then call us back.
        if (num_spells == -1)
            num_spells = SPELLBOOK_SIZE;
        ASSERT(num_spells > 0 && num_spells <= SPELLBOOK_SIZE);

        if (max_levels == -1)
            max_levels = 255;

        if (disc1 == 0 && disc2 == 0)
        {
            if (!_get_weighted_discs(completely_random, god, disc1, disc2))
            {
                if (completely_random)
                    return (false);

                // Rather than give up at this point, choose schools randomly.
                // This way, an acquirement won't fail once the player has
                // seen all spells.
                if (!_get_weighted_discs(true, god, disc1, disc2))
                    return (false);
            }
        }
        else if (disc2 == 0)
            disc2 = disc1;

        ASSERT(disc1 < (1 << (SPTYP_LAST_EXPONENT + 1)));
        ASSERT(disc2 < (1 << (SPTYP_LAST_EXPONENT + 1)));
        ASSERT(count_bits(disc1) == 1 && count_bits(disc2) == 1);

        int disc1_pos = 0, disc2_pos = 0;
        for (int i = 0; i <= SPTYP_LAST_EXPONENT; i++)
        {
            if (disc1 & (1 << i))
                disc1_pos = i;
            if (disc2 & (1 << i))
                disc2_pos = i;
        }

        book.plus  = num_spells | (max_levels << 8);
        book.plus2 = disc1_pos  | (disc2_pos  << 8);

        book.sub_type = BOOK_RANDART_THEME;
        return (make_item_randart(book));
    }

    // Re-read spell and owner, if applicable.
    if (incl_spell == SPELL_NO_SPELL && book.props.exists("spell"))
        incl_spell = (spell_type) book.props["spell"].get_long();

    if (owner.empty() && book.props.exists("owner"))
        owner = book.props["owner"].get_string();

    // We're being called from make_item_randart()
    ASSERT(book.sub_type == BOOK_RANDART_THEME);
    ASSERT(disc1 == 0 && disc2 == 0);
    ASSERT(num_spells == -1 && max_levels == -1);

    num_spells = book.plus & 0xFF;
    ASSERT(num_spells > 0 && num_spells <= SPELLBOOK_SIZE);

    max_levels = (book.plus >> 8) & 0xFF;
    ASSERT(max_levels > 0);

    int disc1_pos = book.plus2 & 0xFF;
    ASSERT(disc1_pos <= SPTYP_LAST_EXPONENT);
    disc1 = 1 << disc1_pos;

    int disc2_pos = (book.plus2 >> 8) & 0xFF;
    ASSERT(disc2_pos <= SPTYP_LAST_EXPONENT);
    disc2 = 1 << disc2_pos;

    int god_discard        = 0;
    int uncastable_discard = 0;

    std::vector<spell_type> spell_list;
    _get_spell_list(spell_list, disc1, disc2, god, !completely_random,
                    god_discard, uncastable_discard);

    if (num_spells > (int) spell_list.size())
        num_spells = spell_list.size();

    spell_type chosen_spells[SPELLBOOK_SIZE];
    for (int i = 0; i < SPELLBOOK_SIZE; i++)
        chosen_spells[i] = SPELL_NO_SPELL;

    _get_weighted_spells(completely_random, god, disc1, disc2,
                         num_spells, max_levels, spell_list, chosen_spells);

    if (!is_valid_spell(incl_spell))
        incl_spell = SPELL_NO_SPELL;
    else
    {
        bool is_included = false;
        for (int i = 0; i < SPELLBOOK_SIZE; i++)
        {
            if (chosen_spells[i] == incl_spell)
            {
                is_included = true;
                break;
            };
        }
        if (!is_included)
            chosen_spells[0] = incl_spell;
    }

    std::sort(chosen_spells, chosen_spells + SPELLBOOK_SIZE, _compare_spells);
    ASSERT(chosen_spells[0] != SPELL_NO_SPELL);

    CrawlHashTable &props = book.props;
    props.erase(SPELL_LIST_KEY);
    props[SPELL_LIST_KEY].new_vector(SV_LONG).resize(SPELLBOOK_SIZE);

    CrawlVector &spell_vec = props[SPELL_LIST_KEY].get_vector();
    spell_vec.set_max_size(SPELLBOOK_SIZE);

    // Count how often each spell school appears in the book.
    int count[SPTYP_LAST_EXPONENT+1];
    for (int k = 0; k <= SPTYP_LAST_EXPONENT; k++)
        count[k] = 0;

    for (int i = 0; i < SPELLBOOK_SIZE; i++)
    {
        if (chosen_spells[i] == SPELL_NO_SPELL)
            continue;

        for (int k = 0; k <= SPTYP_LAST_EXPONENT; k++)
            if (spell_typematch( chosen_spells[i], 1 << k ))
                count[k]++;
    }

    // Remember the two dominant spell schools ...
    int max1 = 0;
    int max2 = 0;
    int num1 = 1;
    int num2 = 0;
    for (int k = 1; k <= SPTYP_LAST_EXPONENT; k++)
    {
        if (count[k] > count[max1])
        {
            max2 = max1;
            num2 = num1;
            max1 = k;
            num1 = 1;
        }
        else
        {
            if (count[k] == count[max1])
                num1++;

            if (max2 == max1 || count[k] > count[max2])
            {
                max2 = k;
                if (count[k] == count[max1])
                    num2 = num1;
                else
                    num2 = 1;
            }
            else if (count[k] == count[max2])
                num2++;
        }
    }

    // If there are several "secondary" disciplines with the same count
    // ignore all of them. Same, if the secondary discipline appears only once.
    if (num2 > 1 && count[max1] > count[max2] || count[max2] < 2)
        max2 = max1;

    // Remove spells that don't fit either discipline.
    _remove_nondiscipline_spells(chosen_spells, 1 << max1, 1 << max2,
                                 incl_spell);

    // ... and change disc1 and disc2 accordingly.
    disc1 = 1 << max1;
    if (max1 == max2)
        disc2 = disc1;
    else
        disc2 = 1 << max2;

    int highest_level = 0;
    int lowest_level  = 10;
    bool all_spells_disc1 = true;

    // Finally fill the spell vector.
    for (int i = 0; i < SPELLBOOK_SIZE; i++)
    {
        spell_vec[i] = (long) chosen_spells[i];
        int diff = spell_difficulty(chosen_spells[i]);
        if (diff > highest_level)
            highest_level = diff;
        else if (diff < lowest_level)
            lowest_level = diff;

        if (all_spells_disc1 && is_valid_spell(chosen_spells[i])
            && !spell_typematch( chosen_spells[i], disc1 ))
        {
            all_spells_disc1 = false;
        }
    }

    // Every spell in the book is of school disc1.
    if (disc1 == disc2)
        all_spells_disc1 = true;

    // If the owner hasn't been set already use
    // a) the god's name for god gifts (only applies to Sif Muna and Xom),
    // b) a name depending on the spell disciplines, for pure books
    // c) a random name (all god gifts not named earlier)
    // d) an applicable god's name
    // ... else leave it unnamed (around 57% chance for non-god gifts)
    if (owner.empty())
    {
        const bool god_gift = (god != GOD_NO_GOD);
        if (god_gift && !one_chance_in(4))
            owner = god_name(god, false);
        else if (god_gift && one_chance_in(3) || one_chance_in(5))
        {
            bool highlevel = (highest_level >= 7 + random2(3)
                              && (lowest_level > 1 || coinflip()));

            if (disc1 != disc2)
            {
                std::string schools[2];
                schools[0] = spelltype_long_name(disc1);
                schools[1] = spelltype_long_name(disc2);
                std::sort(schools, schools + 2);
                std::string lookup = schools[0] + " " + schools[1];

                if (highlevel)
                    owner = getRandNameString("highlevel " + lookup + " owner");

                if (owner.empty() || owner == "__NONE")
                    owner = getRandNameString(lookup + " owner");

                if (owner == "__NONE")
                    owner = "";
            }

            if (owner.empty() && all_spells_disc1)
            {
                std::string lookup = spelltype_long_name(disc1);
                if (highlevel && disc1 == disc2)
                    owner = getRandNameString("highlevel " + lookup + " owner");

                if (owner.empty() || owner == "__NONE")
                    owner = getRandNameString(lookup + " owner");

                if (owner == "__NONE")
                    owner = "";
            }
        }

        if (owner.empty())
        {
            if (god_gift || one_chance_in(5)) // Use a random name.
                owner = make_name(random_int(), false);
            else if (!god_gift && one_chance_in(9))
            {
                god = GOD_SIF_MUNA;
                switch (disc1)
                {
                case SPTYP_NECROMANCY:
                    if (all_spells_disc1 && !one_chance_in(6))
                        god = GOD_KIKUBAAQUDGHA;
                    break;
                case SPTYP_SUMMONING:
                case SPTYP_CONJURATION:
                    if ((all_spells_disc1 || disc2 == SPTYP_SUMMONING
                         || disc2 == SPTYP_CONJURATION) && !one_chance_in(4))
                    {
                        god = GOD_VEHUMET;
                    }
                    break;
                default:
                    break;
                }
                owner = god_name(god, false);
            }
        }
    }

    std::string name = "";

    if (!owner.empty())
    {
        name = apostrophise(owner) + " ";
        book.props["is_named"].get_bool() = true;
    }
    else
        book.props["is_named"].get_bool() = false;

    // Sometimes use a completely random title.
    std::string bookname = "";
    if (owner == "Xom" && !one_chance_in(20))
        bookname = getRandNameString("Xom_book_title");
    else if (one_chance_in(20) && (owner.empty() || one_chance_in(3)))
        bookname = getRandNameString("random_book_title");

    if (!bookname.empty())
        name += getRandNameString("book_noun") + " of " + bookname;
    else
    {
        // Give a name that reflects the primary and secondary
        // spell disciplines of the spells contained in the book.
        name += getRandNameString("book_name") + " ";

        // For the actual name there's a 66% chance of getting something like
        //  <book> of the Fiery Traveller (Translocation/Fire), else
        //  <book> of Displacement and Flames.
        std::string type_name;
        if (disc1 != disc2 && !one_chance_in(3))
        {
            std::string lookup = spelltype_long_name(disc2);
            type_name = getRandNameString(lookup + " adj");
        }

        if (type_name.empty())
        {
            // No adjective found, use the normal method of combining two nouns.
            type_name = getRandNameString(spelltype_long_name(disc1));
            if (type_name.empty())
                name += spelltype_long_name(disc1);
            else
                name += type_name;

            if (disc1 != disc2)
            {
                name += " and ";
                type_name = getRandNameString(spelltype_long_name(disc2));

                if (type_name.empty())
                    name += spelltype_long_name(disc2);
                else
                    name += type_name;
            }
        }
        else
        {
            bookname = type_name + " ";

            // Add the noun for the first discipline.
            type_name = getRandNameString(spelltype_long_name(disc1));
            if (type_name.empty())
                bookname += spelltype_long_name(disc1);
            else
            {
                if (type_name.find("the ", 0) != std::string::npos)
                {
                    type_name = replace_all(type_name, "the ", "");
                    bookname = "the " + bookname;
                }
                bookname += type_name;
            }
            name += bookname;
        }
    }

    set_artefact_name(book, name);

    // Save primary/secondary disciplines back into the book.
    book.plus  = max1;
    book.plus2 = max2;

    return (true);
}

// Give Roxanne a randart spellbook of the disciplines Transmutations/Earth
// that includes Statue Form and is named after her.
void make_book_Roxanne_special(item_def *book)
{
    int disc =  coinflip() ? SPTYP_TRANSMUTATION : SPTYP_EARTH;
    make_book_theme_randart(*book, disc, 0, 5, 19,
                            SPELL_STATUE_FORM, "Roxanne");
}

bool book_has_title(const item_def &book)
{
    ASSERT(book.base_type == OBJ_BOOKS);

    if (!is_artefact(book))
        return (false);

    return (book.props.exists("is_named")
            && book.props["is_named"].get_bool() == true);
}

bool is_dangerous_spellbook(const int book_type)
{
    switch(book_type)
    {
    case BOOK_NECRONOMICON:
    case BOOK_DEMONOLOGY:
    case BOOK_ANNIHILATIONS:
        return (true);
    default:
        break;
    }
    return (false);
}

bool is_dangerous_spellbook(const item_def &book)
{
    ASSERT(book.base_type == OBJ_BOOKS);
    return is_dangerous_spellbook(book.sub_type);
}
