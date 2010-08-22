/*
 *  File:       spells3.cc
 *  Summary:    Implementations of some additional spells.
 *  Written by: Linley Henzell
 */

#include "AppHdr.h"

#include "spells3.h"
#include "externs.h"

#include "areas.h"
#include "coord.h"
#include "env.h"
#include "libutil.h"
#include "message.h"
#include "shout.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "stuff.h"
#include "terrain.h"
#include "viewmap.h"

bool cast_selective_amnesia(bool force)
{
    if (you.spell_no == 0)
    {
        canned_msg(MSG_NO_SPELLS);
        return (false);
    }

    int keyin = 0;

    // Pick a spell to forget.
    while (true)
    {
        mpr("Forget which spell ([?*] list [ESC] exit)? ", MSGCH_PROMPT);
        keyin = get_ch();

        if (key_is_escape(keyin))
            return (false);

        if (keyin == '?' || keyin == '*')
        {
            keyin = list_spells(false);
            redraw_screen();
        }

        if (!isaalpha(keyin))
            mesclr();
        else
            break;
    }

    const spell_type spell = get_spell_by_letter(keyin);
    const int slot = get_spell_slot_by_letter(keyin);

    if (spell == SPELL_NO_SPELL)
    {
        mpr("You don't know that spell.");
        return (false);
    }

    if (!force
        && random2(you.skills[SK_SPELLCASTING])
           < random2(spell_difficulty(spell)))
    {
        mpr("Oops! This spell sure is a blunt instrument.");
        forget_map(20 + random2(50));
    }
    else
    {
        const int ep_gain = spell_mana(spell);
        del_spell_from_memory_by_slot(slot);

        if (ep_gain > 0)
        {
            inc_mp(ep_gain, false);
            mpr("The spell releases its latent energy back to you as "
                "it unravels.");
        }
    }

    return (true);
}

bool project_noise()
{
    bool success = false;

    coord_def pos(1, 0);
    level_pos lpos;

    mpr("Choose the noise's source (press '.' or delete to select).");
    more();

    // Might abort with SIG_HUP despite !allow_esc.
    if (!show_map(lpos, false, false, false))
        lpos = level_pos::current();
    pos = lpos.pos;

    redraw_screen();

    dprf("Target square (%d,%d)", pos.x, pos.y);

    if (!in_bounds(pos) || !silenced(pos))
    {
        if (in_bounds(pos) && !feat_is_solid(grd(pos)))
        {
            noisy(30, pos);
            success = true;
        }

        if (!silenced(you.pos()))
        {
            if (success)
            {
                mprf(MSGCH_SOUND, "You hear a %svoice call your name.",
                     (!you.see_cell(pos) ? "distant " : ""));
            }
            else
                mprf(MSGCH_SOUND, "You hear a dull thud.");
        }
    }

    return (success);
}
