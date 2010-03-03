#include "AppHdr.h"

#include "godgift.h"

#include "mutation.h"
#include "notes.h"
#include "player.h"
#include "random.h"
#include "religion.h"

void jiyva_maybe_give_mutation()
{
    if (you.piety <= 80 || random2(you.piety) <= 50 || !one_chance_in(4)
        || you.gift_timeout > 0 || !you.can_safely_mutate())
    {
        return;
    }

    simple_god_message(" alters your body.");

    bool success = false;
    const int rand = random2(100);

    if (rand < 40)
        success = mutate(RANDOM_MUTATION, true, false, true);
    else if (rand < 60)
        success = delete_mutation(RANDOM_MUTATION, true, false, true);
    else
        success = mutate(RANDOM_GOOD_MUTATION, true, false, true);

    if (success)
    {
        you.gift_timeout += 15 + roll_dice(2, 4);
        you.num_gifts[you.religion]++;
        take_note(Note(NOTE_GOD_GIFT, you.religion));
    }
    else
        mpr("You feel as though nothing has changed.");
}
