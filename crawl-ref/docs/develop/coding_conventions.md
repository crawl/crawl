Crawl codebase conventions
==========================

Introduction
============

This file documents the style conventions currently in use in the Crawl
codebase, as well as the conventions that new and/or modified code should
conform to. It is explicitly not meant to be a didactic "how to program
effectively" treatise.

* [1 - Commit conventions](#1---commit-conventions)
* [2 - Coding conventions](#2---coding-conventions)

1 - Commit conventions
======================

**Formatting**: Please use max 72 character width for your commit messages.
Include a blank line between the title and the body. In vim, you can enforce
this with `:set tw=72`, and then if needed use `gqap` to manually reflow a
paragraph.

**Commit titles**: you should use a brief but informative commit message that
is interpretable by someone viewing only the commit message with no context.
*Don't* use commit messages like `Fix bug`, *Do* use commit messages like
`Fix Freeze death messages`. There is no specific prescribed format for the
text but if you are in doubt, a convention like
[Conventional Commits](https://www.conventionalcommits.org/en/v1.0.0/) is
welcome and may be helpful. For example,
`fix(tiles): Skip pre-layout message rendering`.

**Commit body**: Your commit body should provide a description of the commit
that is interpretable to someone reading just the commit message out of context.
There may be circumstances where just the commit title can do all the work, but
aside from these, the commit body should never be empty, and if you are unsure,
it is better to provide a description. If you are submitting a PR, please
include at least as much of a description in the commit messages as you do in
the PR text (duplicating info is fine). Once the commit is merged, the PR
discussion is hard to access for future contributors trying to understand a
change, but the commit messages are immediate and tied to the change. If
anything, it is better to be more verbose about what a commit does in the commit
message than in the PR text!

**Referencing issues and reporters**: It is good practice to reference any
issues (either in the mantis bug tracker or in github) that are relevant to or
addressed by the commit, in the commit message. For mantis, this should take the
form of the issue number, and can appear in parentheses in the commit title if
there is space. In the body, you can reference either the number (optionally
with the word `mantis`) or directly link to the issue. For a github issue, you
should prefix the number with `#` so that github will correctly link to it. If
you write text like `Resolves #1234` github will automatically close the
mentioned issue on merging, so if your commit does fully resolve an issue,
please add this! This is safe to use in a PR or branch commit, the issue will
only be closed on merging.

If the report came from an individual, it is good practice to credit them,
using the appropriate username format. If there is space this can be in the
commit title. Example: `Simplify Cleansing Flame description (Yermak)`.

2 - Coding Conventions
======================

**Why**: clean diffs, consistent readability patterns across a large
codebase, and making life easier for others: e.g. contributors with
low vision or other accessibility constraints, future (perhaps very
distant future) contributors who may need to decipher your code out
of context.

**TLDR version**: spaces, 4 space indents, 80 columns, brackets on
their own line, use the `checkwhite` and `unbrace` scripts to fix
spacing/bracketing issues or your commit will error, write useful
comments.

**Please fix old code** that does not meet these guidelines!

* [A. Indenting and spacing](#a-indenting-and-spacing)
* [B. Logical Operators](#b-logical-operators)
* [C. Variable Naming](#c-variable-naming)
* [D. Braces](#d-braces)
* [E. Commenting](#e-commenting)
* [F. Macro Usage](#f-macro-usage)
* [G. Spaces Around Parentheses](#g-spaces-around-parentheses)
* [H. Return statements](#h-return-statements)

A. Indenting and spacing
------------------------

Use 4 spaces to indent, and indent with spaces only (no tabs).

* Do not use trailing whitespace on a line, including on empty lines.
* Use unix line endings in your editor.
* Don't use French spacing.
* The end of a file should consist of exactly one newline.

**Note**: the script in `crawl-ref/source/util/checkwhite` can be used to
check as well as fix many mechanical aspects of spacing. It doesn't enforce
the more stylistic indent-size guidelines here, but pretty much everything
else in this section is enforced. This script is run as a CI script, so
your commits will error if they do not meet whitespace guidelines.

**Note**: Existing Lua code in particular often has used 2 space indents. New
Lua code should use 4 space indents; feel free to convert old code, but please
do so in a separate commit from any actual code changes.

Methods
-------

If the parameter list of a method runs longer than a line length (80 columns),
the remaining parameters are usually indented in the lines below. Among other
reasons, this convention is very helpful to low-vision contributors.

    static void replace_area(int sx, int sy, int ex, int ey,
                             dungeon_feature_type replace,
                             dungeon_feature_type feature, unsigned mapmask)
    {
        [...]
    }

The same is true when a method is called:

        // place guardian {dlb}:
        mons_place(MONS_GUARDIAN_NAGA, BEH_SLEEP, MHITNOT, true,
                   sr.x1 + random2(sr.x2 - sr.x1),
                   sr.y1 + random2(sr.y2 - sr.y1));


There are cases where this is not possible because the parameters themselves
are too long for that, or because the method is already heavily indented,
but otherwise, this convention should be followed.


B. Logical operators
--------------------

Conditionals longer than a line should be indented under the starting bracket.
This probably seems obvious for simple ifs ...

    if (!player_in_branch(BRANCH_COCYTUS)
        && !player_in_branch(BRANCH_SWAMP)
        && !player_in_branch(BRANCH_SHOALS))
    {
        _prepare_water(level_number);
    }

... but it should also be followed for else if conditionals.

    else if (keyin == ESCAPE || keyin == ' '
             || keyin == '\r' || keyin == '\n')
    {
        canned_msg(MSG_OK);
        return false;
    }

Space allowing, logical connectives of different precedence should use nested
indenting.

    case ABIL_MAPPING:          // Sense surroundings mutation
        if (abil.ability == ABIL_MAPPING
            && you.get_mutation_level(MUT_MAPPING) < 3
            && (you.level_type == LEVEL_PANDEMONIUM
                || !player_in_mappable_area()))
        {
            mpr("You feel momentarily disoriented.");
            return false;
        }

If a logical connective needs to be distributed over several lines,
the conjunction/disjunction operators (&&, ||) should be placed at the
beginning of the new line rather than at the end of the old one.

    else if (you.mutation[mutat] >= 3
             && mutat != MUT_STRONG && mutat != MUT_CLEVER
             && mutat != MUT_AGILE && mutat != MUT_WEAK
             && mutat != MUT_DOPEY && mutat != MUT_CLUMSY)
    {
        return false;
    }

Since conjunctions (&&) take precedence over disjunctions (||), pure
conjunctive logical connectives don't need to be bracketed, unless this is
absolutely vital for understanding. (Nested indenting helps here.)

    if (you.skills[SK_ICE_MAGIC] > you.skills[SK_FIRE_MAGIC]
        || you.skills[SK_FIRE_MAGIC] == you.skills[SK_ICE_MAGIC]
           && you.skills[SK_AIR_MAGIC] > you.skills[SK_EARTH_MAGIC])
    {
        book = BOOK_CONJURATIONS_II;
    }

In a switch conditional, the case listings don't have to be indented, though
the conditional statements should be.

                switch (mons_intel(monster_index(mon)))
                {
                case I_HIGH:
                    memory = 100 + random2(200);
                    break;
                case I_NORMAL:
                    memory = 50 + random2(100);
                    break;
                case I_ANIMAL:
                case I_INSECT:
                    memory = 25 + random2(75);
                    break;
                case I_PLANT:
                    memory = 10 + random2(50);
                    break;
                }

Comparisons using the ? shortcut may be indented ...

            mpr(forget_spell() ? "You have forgotten a spell!"
                               : "You get a splitting headache.");

... but really short ones don't have to be.

            beam.thrower = (cause) ? KILL_MISC : KILL_YOU;


C. Variable naming
------------------

When naming variables, use underscores_as_spaces instead of mixedCase. Other
conventions are pointed out by example, below.

Global variables are capitalized and underscored.
Warning: there are currently many globals which don't do this.

    int Some_Global_Variable;

Internal functions are prefixed with underscores.

    static void _remove_from_inventory(item_def* item);

Functions use underscores_as_spaces, but there are currently a lot of
mixedCase functions. (Feel free to convert to underscore_as_spaces while
editing them.)

    void destroy_item(item_def* item)
    {
        // - Variables use underscores too.
        int item_weight = /* ... */;
    }

There's no convention for class/struct names (yet?)

    class TextDB
    {
    public:
        // - No rules for static member functions; they're not used often anyway.
        static void whatever();

        // - Public member functions: named like functions.
        void* get_value();

    private:
        // - Internal member functions: also named like functions.
        void _parse_text_file(const char*);

        // - Member variables get a prefix.
        DB* m_db;

        // - Static member variables get a prefix, too.
        std::vector<DB*> sm_all_dbs;
    };


But structures tend to use underscores.

    struct coord_def
    {
        // - Simple structures don't need the "m_" prefixes
        int x, y;
    };


D. Braces
---------

Braces are always put on their own lines.

    do
    {
        curse_an_item(false);
    }
    while (!one_chance_in(3));


**Note:** the script `crawl-ref/source/util/unbrace` can be used to check
and fix many bracing issues. This script is run as a CI check, so your
commit will produce a build error if it does not meet the bracing
guidelines.

If many comparisons are necessary, this can result in a number of nested
braces. These can sometimes be omitted, as long as the underlying logic isn't
changed, of course. The following assumes that the conditions are followed by
single statements.

If both the condition itself and the conditional code are single line
statements, the braces may be omitted; do-while loops, however, should
always have braces (as in the example above).

    if (item != NULL)
        _remove_from_inventory(item);
    else
        _something_else();

Otherwise, place braces.

        if (tran == TRAN_STATUE || tran == TRAN_ICE_BEAST
            || tran == TRAN_AIR || tran == TRAN_LICH
            || tran == TRAN_SPIDER) // monster spiders don't bleed either
        {
            return false;
        }


    for (int i = 0; i < power_level * 5 + 2; ++i)
    {
        create_monster(result, std::min(power/50, 6),
                       friendly ? BEH_FRIENDLY : BEH_HOSTILE,
                       you.x_pos, you.y_pos, MHITYOU, MONS_PROGRAM_BUG);
    }

Also place braces if this is only the case because of one or more comment
lines.

        for (j = 0; j < num_to_make; j++)
        {
            // places items (eg darts), which will automatically stack
            itrap(beam, i);
        }

In the case of nested if-conditionals, try to combine the conditions, e.g.
instead of

    if (A)
    {
        if (B)
            do_something();
    }

use

    if (A && B)
        do_something();

Place braces as per the conventions above.

Else, whenever if-conditional nesting can't be avoided, always use braces. I
couldn't find an example where that isn't already necessary for logical reasons,
so these should be really rare.

In a row of if-else if-statements or in a switch-case loop, the optional braces
should be used if the bigger part of statements needs braces for logical
reasons or because of one of the conventions above. Otherwise, they may be
omitted.

    if (mons_neutral(monster))
    {
        if (coinflip()) // neutrals speak half as often
            return false;

        prefixes.push_back("neutral");
    }
    else if (mons_friendly(monster))
        prefixes.push_back("friendly");
    else
        prefixes.push_back("hostile");

When for-loops are nested and the outer loop has no further statements, the
braces may be omitted.

    for (int x = 0; x < GXM; x++)
        for (int y = 0; y < GYM; y++)
        {
            if (grd[x][y] == DNGN_LAVA)
                lava_spaces++;
            if (grd[x][y] == DNGN_DEEP_WATER || grd[x][y] == DNGN_SHALLOW_WATER)
                water_spaces++;
        }

The same is true for combined for- and if-conditionals as long as all
statements fill less than four lines.

    for (i = 0; i < MAX_SHOPS; i++)
        if (env.shop[i].type == SHOP_UNASSIGNED)
            break;

If the order of if- and for-conditionals is reversed, however, place braces.

    [...]
    else if (enhanced < 0)
    {
        for (ndx = enhanced; ndx < 0; ndx++)
            power /= 2;
    }


If there are more than three nested statements with optional bracing, use braces
to roughly divide them into halves. (See example below.)

Should such nested code be followed by code other than a closing brace,
leave a free line between them.

    for (int y = 1; y < GYM; ++y)
        for (int x = 1; x < GXM; ++x)
        {
            if (grd[x][y] == feat)
                return coord_def(x, y);
        }

    return coord_def(0, 0);

E. Commenting
-------------

If you feel that a method is complicated enough to be difficult to understand,
or has restrictions or effects that might not be obvious, add explanatory
comments before it. You may sign your comments if you wish to.

    // Note that this function *completely* blocks messaging for monsters
    // distant or invisible to the player ... look elsewhere for a function
    // permitting output of "It" messages for the invisible {dlb}
    // Intentionally avoids info and str_pass now. -- bwr
    bool simple_monster_message(const monsters *monster, const char *event,
                              msg_channel_type channel, int param,
                              description_level_type descrip)
    [...]

Adding explanatory comments to somewhat complicated, already existing methods
is very much welcome, as long as said comments are correct. :)

Suboptimal code should be marked for later revision using the keywords "XXX",
"FIXME" or "TODO", so that other coders can easily find it by searching the
source. Also, some editors will highlight such keywords. Don't forget to add
a comment about what should be changed, or to remove said comment if you fix
the issue.

    // XXX Unbelievably hacky. And to think that my goal was to clean up the code.
    // Identical to find_square, except that input (tx, ty) and output
    // (mfp) are in grid coordinates rather than view coordinates.
    static char find_square_wrapper( int tx, int ty,
    [...]


    if (death_type == KILLED_BY_LEAVING
        || death_type == KILLED_BY_WINNING)
    {
        // TODO: strcat "after reaching level %d"; for LEAVING
        [...]


    // FIXME: implement this for tiles
    void update_monster_pane() {}

Comments should not be used to archive old code. If something is no longer
needed, do not comment it out, simply remove it.

F. Macro Usage
--------------------

If you need to use a macro as a boolean flag, do this:

    #define SOME_DEBUG_FLAG

    #ifdef SOME_DEBUG_FLAG
    do_something();
    #endif

Not this:

    #define SOME_DEBUG_FLAG 1

    #if SOME_DEBUG_FLAG
    do_something();
    #endif

The reason for this is that it's common for such debug flags to be left
undefined, leading to the #if statement having an undefined behaviour. Most sane
compilers will give undefined macros a value of `0` during such `#if`
comparisons, but some compilers throw errors. So, play it safe. Use `#ifdef`.

Another thing to watch for with such flags is this sort of thing:

    #define SOME_FLAG
    #define SOME_OTHER_FLAG

    #ifdef SOME_FLAG
    do_something();
    #elif SOME_OTHER_FLAG
    do_something_else();
    #endif

The `#elif` statement should read `#elif defined(SOME_OTHER_FLAG)`. If the
macro was undefined, this would lead to the same undefined behaviour noted
above. If it was defined as above, the #elif would not function as expected.

G. Spaces around parentheses
----------------------------

Prefer no spaces inside parentheses.

Put a space before opening parenthesis if it's part of a conditional or preceded
by a statement.

    if (trap_count > 0)

    catch (map_load_exception &mload)

 But not if it is the parameter list of a function or method.

    _get_inv_items_to_show(tobeshown, item_selector, excluded_slot);

    tileset.push_back(tile_def(TILE_ITEM_SLOT, TEX_FEAT));

H. Return statements
--------------------

Do not use parentheses on return statements. The parentheses serve no
purpose other than making the statement look like a function call.

    return true;

    return -1;

    return coinflip();

    return form_can_wield(form) || form == TRAN_DRAGON;
