/*
 *  File:       artefact.h
 *  Summary:    Random and unrandom artefact functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */


#ifndef RANDART_H
#define RANDART_H

#include "externs.h"

// NOTE: NO_UNRANDARTS is automatically set by util/art-data.pl
#define NO_UNRANDARTS 76

#define ART_PROPERTIES ARTP_NUM_PROPERTIES

// Reserving the upper bits for later expansion/versioning.
#define RANDART_SEED_MASK  0x00ffffff

#define SPWPN_SINGING_SWORD       UNRAND_SINGING_SWORD
#define SPWPN_WRATH_OF_TROG       UNRAND_TROG
#define SPWPN_SCYTHE_OF_CURSES    UNRAND_CURSES
#define SPWPN_MACE_OF_VARIABILITY UNRAND_VARIABILITY
#define SPWPN_GLAIVE_OF_PRUNE     UNRAND_PRUNE
#define SPWPN_SCEPTRE_OF_TORMENT  UNRAND_TORMENT
#define SPWPN_SWORD_OF_ZONGULDROK UNRAND_ZONGULDROK
#define SPWPN_SWORD_OF_POWER      UNRAND_POWER
#define SPWPN_STAFF_OF_OLGREB     UNRAND_OLGREB
#define SPWPN_VAMPIRES_TOOTH      UNRAND_VAMPIRES_TOOTH
#define SPWPN_STAFF_OF_WUCAD_MU   UNRAND_WUCAD_MU
#define SPWPN_SWORD_OF_CEREBOV    UNRAND_CEREBOV
#define SPWPN_STAFF_OF_DISPATER   UNRAND_DISPATER
#define SPWPN_SCEPTRE_OF_ASMODEUS UNRAND_ASMODEUS

#define SPWPN_START_NOGEN_FIXEDARTS UNRAND_CEREBOV
#define SPWPN_END_FIXEDARTS         UNRAND_ASMODEUS

enum unrand_special_type
{
    UNRANDSPEC_EITHER,
    UNRANDSPEC_NORMAL,
    UNRANDPSEC_SPECIAL
};

// NOTE: This enumeration is automatically generated from art-data.txt
// via util/art-data.pl
enum unrand_type
{
    UNRAND_START = 180,
    UNRAND_DUMMY1 = UNRAND_START,
    UNRAND_SINGING_SWORD,    // Singing Sword
    UNRAND_TROG,             // Wrath of Trog
    UNRAND_VARIABILITY,      // Mace of Variability
    UNRAND_PRUNE,            // Glaive of Prune
    UNRAND_POWER,            // Sword of Power
    UNRAND_OLGREB,           // Staff of Olgreb
    UNRAND_WUCAD_MU,         // Staff of Wucad Mu
    UNRAND_VAMPIRES_TOOTH,   // Vampire's Tooth
    UNRAND_CURSES,           // Scythe of Curses
    UNRAND_TORMENT,          // Sceptre of Torment
    UNRAND_ZONGULDROK,       // Sword of Zonguldrok
    UNRAND_CEREBOV,          // Sword of Cerebov
    UNRAND_DISPATER,         // Staff of Dispater
    UNRAND_ASMODEUS,         // Sceptre of Asmodeus
    UNRAND_BLOODBANE,        // long sword "Bloodbane"
    UNRAND_FLAMING_DEATH,    // scimitar of Flaming Death
    UNRAND_BRILLIANCE,       // mace of Brilliance
    UNRAND_LEECH,            // demon blade "Leech"
    UNRAND_CHILLY_DEATH,     // dagger of Chilly Death
    UNRAND_MORG,             // dagger "Morg"
    UNRAND_FINISHER,         // scythe "Finisher"
    UNRAND_PUNK,             // sling "Punk"
    UNRAND_KRISHNA,          // bow of Krishna "Sharnga"
    UNRAND_SKULLCRUSHER,     // giant club "Skullcrusher"
    UNRAND_GUARD,            // glaive of the Guard
    UNRAND_JIHAD,            // sword of Jihad
    UNRAND_HELLFIRE,         // crossbow "Hellfire"
    UNRAND_DOOM_KNIGHT,      // sword of the Doom Knight
    UNRAND_EOS,              // "Eos"
    UNRAND_BOTONO,           // spear of the Botono
    UNRAND_OCTOPUS_KING,     // trident of the Octopus King
    UNRAND_ARGA,             // mithril axe "Arga"
    UNRAND_ELEMENTAL_STAFF,  // Elemental Staff
    UNRAND_SNIPER,           // hand crossbow "Sniper"
    UNRAND_PIERCER,          // longbow "Piercer"
    UNRAND_BLOWGUN_ASSASSIN, // blowgun of the Assassin
    UNRAND_WYRMBANE,         // Wyrmbane
    UNRAND_SPRIGGANS_KNIFE,  // Spriggan's Knife
    UNRAND_PLUTONIUM_SWORD,  // plutonium sword
    UNRAND_UNDEADHUNTER,     // great mace "Undeadhunter"
    UNRAND_SERPENT_SCOURGE,  // whip "Serpent-Scourge"
    UNRAND_ACCURACY,         // knife of Accuracy
    UNRAND_CRYSTAL_SPEAR,    // Lehudib's crystal spear
    UNRAND_IGNORANCE,        // shield of Ignorance
    UNRAND_AUGMENTATION,     // robe of Augmentation
    UNRAND_THIEF,            // cloak of the Thief
    UNRAND_BULLSEYE,         // shield "Bullseye"
    UNRAND_DYROVEPREVA,      // crown of Dyrovepreva
    UNRAND_MISFORTUNE,       // robe of Misfortune
    UNRAND_FLASH,            // cloak of Flash
    UNRAND_BOOTS_ASSASSIN,   // boots of the Assassin
    UNRAND_LEARS,            // Lear's chain mail
    UNRAND_ZHOR,             // skin of Zhor
    UNRAND_SALAMANDER,       // salamander hide armour
    UNRAND_WAR,              // gauntlets of War
    UNRAND_RESISTANCE,       // shield of Resistance
    UNRAND_FOLLY,            // robe of Folly
    UNRAND_MAXWELL,          // Maxwell's patent armour
    UNRAND_DRAGON,           // mask of the Dragon
    UNRAND_NIGHT,            // robe of Night
    UNRAND_DRAGON_KING,      // armour of the Dragon King
    UNRAND_ALCHEMIST,        // hat of the Alchemist
    UNRAND_FENCERS_GLOVES,   // Fencer's gloves
    UNRAND_STARLIGHT,        // cloak of Starlight
    UNRAND_RATSKIN_CLOAK,    // ratskin cloak
    UNRAND_AIR,              // amulet of the Air
    UNRAND_SHADOWS,          // ring of Shadows
    UNRAND_CEKUGOB,          // amulet of Cekugob
    UNRAND_FOUR_WINDS,       // amulet of the Four Winds
    UNRAND_BLOODLUST,        // necklace of Bloodlust
    UNRAND_SHAOLIN,          // ring of Shaolin
    UNRAND_ROBUSTNESS,       // ring of Robustness
    UNRAND_MAGE,             // ring of the Mage
    UNRAND_SHIELDING,        // brooch of Shielding
    UNRAND_DUMMY2,           // DUMMY UNRANDART 2
    UNRAND_LAST = UNRAND_DUMMY2
};

// The following unrandart bits were taken from $pellbinder's mon-util
// code (see mon-util.h & mon-util.cc) and modified (LRH).
struct unrandart_entry
{
    const char *name;        // true name of unrandart (max 31 chars)
    const char *unid_name;   // un-id'd name of unrandart (max 31 chars)

    object_class_type base_type;        // class of ura
    int sub_type;        // type of ura
    int plus;        // plus of ura
    int plus2;       // plus2 of ura
    int colour;       // colour of ura
    short prpty[ART_PROPERTIES];

    // special description added to 'v' command output (max 31 chars)
    const char *desc;
    // special description added to 'v' command output (max 31 chars)
    const char *desc_id;
    // special description added to 'v' command output (max 31 chars)
    const char *desc_end;
};

bool is_known_artefact( const item_def &item );
bool is_artefact( const item_def &item );
bool is_random_artefact( const item_def &item );
bool is_unrandom_artefact( const item_def &item );
bool is_special_unrandom_artefact( const item_def &item );

unique_item_status_type get_unique_item_status(const item_def& item);
unique_item_status_type get_unique_item_status(int unrand_index);
void set_unique_item_status(const item_def& item,
                            unique_item_status_type status );
void set_unique_item_status(int unrand_index,
                            unique_item_status_type status );

/* ***********************************************************************
 * called from: itemname
 * *********************************************************************** */
std::string get_artefact_name( const item_def &item );

/* ***********************************************************************
 * called from: spl-book
 * *********************************************************************** */
void set_artefact_name( item_def &item, const std::string &name );
void set_artefact_appearance( item_def &item, const std::string &appear );

/* ***********************************************************************
 * called from: effects
 * *********************************************************************** */
std::string artefact_name( const item_def &item, bool appearance = false );

/* ***********************************************************************
 * called from: describe
 * *********************************************************************** */
const char *unrandart_descrip( int which_descrip, const item_def &item );

/* ***********************************************************************
 * called from: dungeon makeitem
 * *********************************************************************** */
int find_okay_unrandart(unsigned char aclass, unsigned char atype = OBJ_RANDOM,
                        unrand_special_type specialness = UNRANDSPEC_EITHER,
                        bool in_abyss = false);

typedef FixedVector< int, ART_PROPERTIES >  artefact_properties_t;
typedef FixedVector< bool, ART_PROPERTIES > artefact_known_props_t;

/* ***********************************************************************
 * called from: describe - fight - it_use2 - item_use - player
 * *********************************************************************** */
void artefact_desc_properties( const item_def        &item,
                              artefact_properties_t &proprt,
                              artefact_known_props_t &known,
                              bool force_fake_props = false);

void artefact_wpn_properties( const item_def       &item,
                             artefact_properties_t &proprt,
                             artefact_known_props_t &known );

void artefact_wpn_properties( const item_def &item,
                             artefact_properties_t &proprt );

int artefact_wpn_property( const item_def &item, artefact_prop_type prop,
                          bool &known );

int artefact_wpn_property( const item_def &item, artefact_prop_type prop );

int artefact_known_wpn_property( const item_def &item,
                                 artefact_prop_type prop );

int artefact_wpn_num_props( const item_def &item );
int artefact_wpn_num_props( const artefact_properties_t &proprt );

void artefact_wpn_learn_prop( item_def &item, artefact_prop_type prop );
bool artefact_wpn_known_prop( const item_def &item, artefact_prop_type prop );

/* ***********************************************************************
 * called from: dungeon
 * *********************************************************************** */
bool make_item_blessed_blade( item_def &item );
bool make_item_randart( item_def &item );
bool make_item_unrandart( item_def &item, int unrand_index );

/* ***********************************************************************
 * called from: randart - debug
 * *********************************************************************** */
bool randart_is_bad( const item_def &item );
bool randart_is_bad( const item_def &item, artefact_properties_t &proprt );

/* ***********************************************************************
 * called from: items
 * *********************************************************************** */
int find_unrandart_index(const item_def& artefact);

unrandart_entry* get_unrand_entry(int unrand_index);

unrand_special_type get_unrand_specialness(int unrand_index);
unrand_special_type get_unrand_specialness(const item_def &item);

/* ***********************************************************************
 * called from: debug
 * *********************************************************************** */
void artefact_set_properties( item_def              &item,
                              artefact_properties_t &proprt );
void artefact_set_property( item_def           &item,
                            artefact_prop_type  prop,
                            int                 val );

/* ***********************************************************************
 * called from: mapdef
 * *********************************************************************** */
int get_unrandart_num( const char *name );
#endif
