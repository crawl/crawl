/*
 *  File:       mutation.cc
 *  Summary:    Functions for handling player mutations.
 *  Written by: Linley Henzell
 */


#ifndef MUTATION_H
#define MUTATION_H

#include <string>

class formatted_string;

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
    int tiers[3];
};

struct demon_mutation_info
{
    mutation_type mut;
    int tier;
    int facet;

    demon_mutation_info(mutation_type m, int t, int f)
        : mut(m), tier(t), facet(f) { }
};

struct mutation_def
{
    mutation_type mutation;
    short         rarity;    // Rarity of the mutation.
    short         levels;    // The number of levels of the mutation.
    bool          bad;       // A mutation that's more bad than good. Xom uses
                             // this to decide which mutations to hand out as
                             // rewards.
    bool          physical;  // A mutation affecting a character's outward
                             // appearance.
    const char*   short_desc;// What appears on the '%' screen.
    const char*   have[3];   // What appears on the 'A' screen.
    const char*   gain[3];   // Message when you gain the mutation.
    const char*   lose[3];   // Message when you lose the mutation.
    const char*   wizname;   // For gaining it in wizmode.
};

void init_mut_index();

bool is_valid_mutation(mutation_type mut);
bool is_body_facet(mutation_type mut);
const mutation_def& get_mutation_def(mutation_type mut);

void fixup_mutations();

bool mutate(mutation_type which_mutation, bool failMsg = true,
            bool force_mutation = false, bool god_gift = false,
            bool stat_gain_potion = false, bool demonspawn = false);

inline bool give_bad_mutation(bool failMsg = true, bool force_mutation = false)
{
    return (mutate(RANDOM_BAD_MUTATION, failMsg, force_mutation,
                   false, false, false));
}

void display_mutations();
bool mutation_is_fully_active(mutation_type mut);
formatted_string describe_mutations();

bool delete_mutation(mutation_type which_mutation, bool failMsg = true,
                     bool force_mutation = false, bool god_gift = false,
                     bool disallow_mismatch = false);

bool delete_all_mutations();

std::string mutation_name(mutation_type which_mutat, int level = -1,
                          bool colour = false);

void roll_demonspawn_mutations();

bool perma_mutate(mutation_type which_mut, int how_much);
int how_mutated(bool all = false, bool levels = false);

void check_demonic_guardian();
void check_antennae_detect();
int handle_pbd_corpses(bool do_rot);

#endif
