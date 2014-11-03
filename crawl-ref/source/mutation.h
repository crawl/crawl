/**
 * @file
 * @brief Functions for handling player mutations.
**/

#ifndef MUTATION_H
#define MUTATION_H

#include <string>

class formatted_string;

enum mutation_activity_type
{
    MUTACT_INACTIVE, // form-based mutations in most forms
    MUTACT_PARTIAL,  // scales on statues
    MUTACT_FULL,     // other mutations
};

enum mutation_permanence_class
{
    // Temporary mutations wear off after awhile
    MUTCLASS_TEMPORARY,
    // Normal mutations, permanent unless cured
    MUTCLASS_NORMAL,
    // Innate, permanent traits, like draconian breath
    MUTCLASS_INNATE
};

struct body_facet_def
{
    equipment_type eq;
    mutation_type mut;
    int level_lost;
};

struct facet_def
{
    int tier;
    mutation_type muts[3];
    int when[3];
};

struct demon_mutation_info
{
    mutation_type mut;
    int when;
    int facet;

    demon_mutation_info(mutation_type m, int w, int f)
        : mut(m), when(w), facet(f) { }
};

enum mut_use_type // Which gods/effects use these mutations?
{
    MU_USE_NONE,     // unused
    MU_USE_GOOD,     // used by benemut etc
    MU_USE_MIN     = MU_USE_GOOD,
    MU_USE_BAD,      // used by malmut etc
    MU_USE_JIYVA,    // jiyva-only muts
    MU_USE_QAZLAL,   // qazlal wrath
    MU_USE_XOM,      // xom being xom
    MU_USE_CORRUPT,  // wretched stars
    MU_USE_RU,       // Ru sacrifice muts
    NUM_MU_USE
};

#define MFLAG(mt) (1 << (mt))
#define MUTFLAG_NONE    MFLAG(MU_USE_NONE)
#define MUTFLAG_GOOD    MFLAG(MU_USE_GOOD)
#define MUTFLAG_BAD     MFLAG(MU_USE_BAD)
#define MUTFLAG_JIYVA   MFLAG(MU_USE_JIYVA)
#define MUTFLAG_QAZLAL  MFLAG(MU_USE_QAZLAL)
#define MUTFLAG_XOM     MFLAG(MU_USE_XOM)
#define MUTFLAG_CORRUPT MFLAG(MU_USE_CORRUPT)
#define MUTFLAG_RU      MFLAG(MU_USE_RU)

struct mutation_def
{
    mutation_type mutation;
    short       weight;     // Commonality of the mutation; bigger = appears
                            // more often.
    short       levels;     // The number of levels of the mutation.
    unsigned int  uses;     // Bitfield holding types of effects that grant
                            // this mutation. (MFLAG)
    bool        form_based; // A mutation that is suppressed when shapechanged.
    const char* short_desc; // What appears on the '%' screen.
    const char* have[3];    // What appears on the 'A' screen.
    const char* gain[3];    // Message when you gain the mutation.
    const char* lose[3];    // Message when you lose the mutation.
    const char* desc;       // A descriptive phrase that can be used in a sentence.
};

void init_mut_index();

bool is_body_facet(mutation_type mut);
const mutation_def& get_mutation_def(mutation_type mut);
bool undead_mutation_rot();

bool mutate(mutation_type which_mutation, const string &reason,
            bool failMsg = true,
            bool force_mutation = false, bool god_gift = false,
            bool beneficial = false,
            mutation_permanence_class mutclass = MUTCLASS_NORMAL,
            bool no_rot = false);

void display_mutations();
int mut_check_conflict(mutation_type mut);
mutation_activity_type mutation_activity_level(mutation_type mut);
string describe_mutations(bool center_title);

bool delete_mutation(mutation_type which_mutation, const string &reason,
                     bool failMsg = true,
                     bool force_mutation = false, bool god_gift = false,
                     bool disallow_mismatch = false);

bool delete_all_mutations(const string &reason);

const char* mutation_name(mutation_type mut);
const char* mutation_desc_for_text(mutation_type mut);
string mutation_desc(mutation_type which_mutat, int level = -1,
                          bool colour = false, bool is_sacrifice = false);

void roll_demonspawn_mutations();

bool perma_mutate(mutation_type which_mut, int how_much, const string &reason);
bool temp_mutate(mutation_type which_mut, const string &reason);
int how_mutated(bool innate = false, bool levels = false);

void check_demonic_guardian();
void check_monster_detect();
int handle_pbd_corpses();
equipment_type beastly_slot(int mut);
bool physiology_mutation_conflict(mutation_type mutat);
int augmentation_amount();

bool delete_temp_mutation();

#endif
