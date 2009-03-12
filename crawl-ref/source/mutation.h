/*
 *  File:       mutation.cc
 *  Summary:    Functions for handling player mutations.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */


#ifndef MUTATION_H
#define MUTATION_H

#include <string>

class formatted_string;

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
    const char*   have[3];   // What appears on the 'A' screen.
    const char*   gain[3];   // Message when you gain the mutation.
    const char*   lose[3];   // Message when you lose the mutation.
    const char*   wizname;   // For gaining it in wizmode.
};

const mutation_def& get_mutation_def(mutation_type mut);

void fixup_mutations();

bool mutate(mutation_type which_mutation, bool failMsg = true,
            bool force_mutation = false, bool god_gift = false,
            bool stat_gain_potion = false, bool demonspawn = false,
            bool non_fatal = false);

void display_mutations();
bool mutation_is_fully_active(mutation_type mut);
formatted_string describe_mutations();

bool delete_mutation(mutation_type which_mutation, bool failMsg = true,
                     bool force_mutation = false, bool non_fatal = false);

std::string mutation_name(mutation_type which_mutat, int level = -1,
                          bool colour = false);

bool give_bad_mutation(bool failMsg = true, bool force_mutation = false,
                       bool non_fatal = false);

void demonspawn();

bool perma_mutate(mutation_type which_mut, int how_much);
int how_mutated(bool all = false, bool levels = false);

#endif
