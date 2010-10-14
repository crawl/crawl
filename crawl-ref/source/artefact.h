/*
 *  File:       artefact.h
 *  Summary:    Random and unrandom artefact functions.
 *  Written by: Linley Henzell
 */


#ifndef RANDART_H
#define RANDART_H

#include "externs.h"

struct bolt;

#include "art-enum.h"

#define ART_PROPERTIES ARTP_NUM_PROPERTIES

// Reserving the upper bits for later expansion/versioning.
#define RANDART_SEED_MASK  0x00ffffff

#define KNOWN_PROPS_KEY     "artefact_known_props"
#define ARTEFACT_PROPS_KEY  "artefact_props"
#define ARTEFACT_NAME_KEY   "artefact_name"
#define ARTEFACT_APPEAR_KEY "artefact_appearance"

enum unrand_flag_type
{
    UNRAND_FLAG_NONE             = 0x00,
    UNRAND_FLAG_SPECIAL          = 0x01,
    UNRAND_FLAG_HOLY             = 0x02,
    UNRAND_FLAG_UNHOLY           = 0x04,
    UNRAND_FLAG_EVIL             = 0x08,
    UNRAND_FLAG_UNCLEAN          = 0x10,
    UNRAND_FLAG_CHAOTIC          = 0x20,
    UNRAND_FLAG_CORPSE_VIOLATING = 0x40,
    UNRAND_FLAG_NOGEN            = 0x80,
    // Warning!  Any further extension and you'll need to change the field from
    // a char.  What a loss of 82*sizeof() bytes...
};

enum setup_missile_type
{
    SM_CONTINUE,
    SM_FINISHED,
    SM_CANCEL,
};

// The following unrandart bits were taken from $pellbinder's mon-util
// code (see mon-util.h & mon-util.cc) and modified (LRH).
struct unrandart_entry
{
    const char *name;        // true name of unrandart
    const char *unid_name;   // un-id'd name of unrandart

    object_class_type base_type;
    uint8_t           sub_type;
    short             plus;
    short             plus2;
    uint8_t           colour;       // colour of ura

    short         value;
    uint8_t       flags;

    short prpty[ART_PROPERTIES];

    // special description added to 'v' command output
    const char *desc;     // replaces un-ided (and default ided) db entry
    const char *desc_id;  // replaces ided
    const char *desc_end; // appended to ided

    void (*equip_func)(item_def* item, bool* show_msgs, bool unmeld);
    void (*unequip_func)(const item_def* item, bool* show_msgs);
    void (*world_reacts_func)(item_def* item);
    // An item can't be a melee weapon and launcher at the same time, so have
    // the functions relevant to those item types share a union.
    union
    {
        void (*melee_effects)(item_def* item, actor* attacker,
                              actor* defender, bool mondied);
        setup_missile_type (*launch)(item_def* item, bolt* beam,
                                     std::string* ammo_name, bool* returning);
    } fight_func;
    bool (*evoke_func)(item_def *item, int* pract, bool* did_work,
                       bool* unevokable);
};

bool is_known_artefact(const item_def &item);
bool is_artefact(const item_def &item);
bool is_random_artefact(const item_def &item);
bool is_unrandom_artefact(const item_def &item);
bool is_special_unrandom_artefact(const item_def &item);

void artefact_fixup_props(item_def &item);

unique_item_status_type get_unique_item_status(const item_def& item);
unique_item_status_type get_unique_item_status(int unrand_index);
void set_unique_item_status(const item_def& item,
                            unique_item_status_type status);
void set_unique_item_status(int unrand_index,
                            unique_item_status_type status);

std::string get_artefact_name(const item_def &item, bool force_known = false);

void set_artefact_name(item_def &item, const std::string &name);
void set_artefact_appearance(item_def &item, const std::string &appear);

std::string artefact_name(const item_def &item, bool appearance = false);

const char *unrandart_descrip(int which_descrip, const item_def &item);

int find_okay_unrandart(uint8_t aclass, uint8_t atype = OBJ_RANDOM,
                        bool in_abyss = false);

typedef FixedVector< int, ART_PROPERTIES >  artefact_properties_t;
typedef FixedVector< bool, ART_PROPERTIES > artefact_known_props_t;

void artefact_desc_properties(const item_def        &item,
                              artefact_properties_t &proprt,
                              artefact_known_props_t &known,
                              bool force_fake_props = false);

void artefact_wpn_properties(const item_def       &item,
                             artefact_properties_t &proprt,
                             artefact_known_props_t &known);

void artefact_wpn_properties(const item_def &item,
                             artefact_properties_t &proprt);

int artefact_wpn_property(const item_def &item, artefact_prop_type prop,
                          bool &known);

int artefact_wpn_property(const item_def &item, artefact_prop_type prop);

int artefact_known_wpn_property(const item_def &item,
                                 artefact_prop_type prop);

void artefact_wpn_learn_prop(item_def &item, artefact_prop_type prop);
bool artefact_wpn_known_prop(const item_def &item, artefact_prop_type prop);

bool make_item_randart(item_def &item, bool force_mundane = false);
bool make_item_unrandart(item_def &item, int unrand_index);

bool randart_is_bad(const item_def &item);
bool randart_is_bad(const item_def &item, artefact_properties_t &proprt);

int find_unrandart_index(const item_def& artefact);

unrandart_entry* get_unrand_entry(int unrand_index);

void artefact_set_properties(item_def              &item,
                              const artefact_properties_t &proprt);
void artefact_set_property(item_def           &item,
                            artefact_prop_type  prop,
                            int                 val);

int get_unrandart_num(const char *name);

void cheibriados_make_item_ponderous(item_def &item);

#endif
