# Creating a DCSS Custom Background

## Introduction

Backgrounds are one of the simplest gameplay elements to modify in the DCSS
codebase, as they only really determine things at character creation. Within the
code itself, they're usually referred to as jobs, not backgrounds, so I'll be
using these terms interchangeably. There's no code that checks for your starting
job later on, they just boost your stats a little and supply you with your
starting gear. I'll write up how to add a new background to the game here.

Note that this is meant to be a technical explanation of how to do this, and not
a game design discussion. Getting a custom background added to the stable
release may be difficult, but the mechanics of creating local modifications are
fairly straightforward.

## Example Background

We're going to add in a Shepherd job today. I feel there's a lack of peaceful,
relaxing jobs in the game, so we'll fill that space with our new job.

First we need to edit **job-type.h** to tell the game our new job exists. Search
for "enum job_type", and we'll add an entry right before the NUM_JOBS line.

This should look something like:

~~~
    JOB_ABYSSAL_KNIGHT,
#if TAG_MAJOR_VERSION == 34
    JOB_JESTER,
#endif
    JOB_SHEPHERD,
    NUM_JOBS,              // always after the last job
~~~

### Special Variables

Two side-notes here. First, you'll see that TAG_MAJOR_VERSION thing all over the
code base. Don't bother adding it in yourself. It's mostly used as shorthand to
mark code that isn't being used or could be removed. Second, the NUM_JOBS entry
in the enum list is a special value used pretty often in the codebase. Make sure
your new entry is right above NUM_JOBS for save compatibility reasons. Changing
the list in any other way might cause saved characters to change jobs mid-game
or throw errors later.

### Background Definition

Second, we need to add the actual data for the background. For this, we go into
**job-data.h**. This struct in this file is reasonably well-explained:

~~~
struct job_def
  {
    const char* abbrev; ///< Two-letter abbreviation
    const char* name; ///< Long name
    int s, i, d; ///< Starting Str, Int, and Dex
    /// Which species are good at it
    /// No recommended species = job is disabled
    vector<species_type> recommended_species;
    /// What spells start out in their library?
    /// The first spell in the list will be memorised at the start of the game,
    /// if it's level 1 and not useless.
    vector<spell_type> library;
    /// Guaranteed starting equipment. Uses vault spec syntax, with the plus:,
    /// charges:, q:, and ego: tags supported.
    vector<string> equipment;
    weapon_choice wchoice; ///< how the weapon is chosen, if any
    vector<pair<skill_type, int>> skills; ///< starting skills
  };
~~~

The best thing to do is to copy an existing class and edit its entries; just to
make sure you get the structure right. Also note that this file sorts the
entries alphabetically, so we'll follow suit here. Let's fill in our typical
herd-watcher:

~~~
// First, the basics:
{ JOB_SHEPHERD, {
    "Sh", "Shepherd",

    // Fieldwork keeps shepherds in good shape, and gives them lots of time to
    // think.
    12, 11, 12,

    // Most shepherds are common folk.
    { SP_HUMAN, SP_MINOTAUR, SP_TENGU, SP_TROLL, SP_NAGA, SP_VINE_STALKER, },

    // Shepherds aren't skilled at magic, beyond a cantrip or two.
    { SPELL_CHAIN_LIGHTNING, SPELL_SHATTER,
      SPELL_POLAR_VORTEX, SPELL_FIRE_STORM },

    // They should have a light breezy garment, something to read to
    // pass the time, and a staff to protect their flock
    { "gold dragon scales plus:9", "scroll of acquirement q:10",
      "greatsword plus:9 ego:flaming" },
    WCHOICE_NONE,

    // and just a couple skill points in stuff you might do around a field.
    { { SK_FIGHTING, 6 }, { SK_SUMMONINGS, 8 }, { SK_SPELLCASTING, 3 },
      { SK_DODGING, 3 }, { SK_ARMOUR, 4 } { SK_STEALTH, 1 }, },
} },
~~~

That looks like what I expect from a shepherd: lightly armed, lightly armoured,
and just a speck of potential. Other bits of code will ensure any weapons and
armour are automatically equipped at creation (but not staves or other
equippables!).

If you skip one of the above steps, you'll hit an ASSERT error after running the
game. This is the typical way DCSS handles errors: check for things that
shouldn't be and crash early with a clear message instead of crashing later with
a cryptic code dump.

### New Game Setup

The last step is to add the background to the start-up menu. This is handled in
**newgame.cc**, and we need to edit the jobs_order[] array to add ours in. I
think Shepherds count as a Adventurer, so I'll update the entry for them:

~~~
{
    "Adventurer",
    coord_def(0, 7), 15,
    { JOB_ARTIFICER, JOB_WANDERER, JOB_DELVER, JOB_SHEPHERD }
},
~~~

#### Menu and GUI Issues

Take note of the coord_def variable inside **newgame.cc**. The background
selection menu is typically laid out in three columns. Coord_def(x, y)
identifies these locations. X of 0, 1, and 2 identifies the first, second and
third columns. Y identifies the vertical start of the section within the column.
As an example, if you were going to add a new Zealot background, you would have
to increment the Y coordinate of the Warrior-mage section, which sits below it
in the second column (where X == 1). In the example below, the Y coordinate of
Warrior-mage has been incremented to 5 (from a default of 4) to allow room for a
new background (JOB_CUSTOM) added to the Zealot section.

~~~
{
        "Zealot",
        coord_def(1, 0), 25,
        { JOB_BERSERKER, JOB_CINDER_ACOLYTE,
          JOB_CHAOS_KNIGHT, JOB_CUSTOM }
},
{
        "Warrior-mage",
        coord_def(1, 5), 26,
        { JOB_TRANSMUTER, JOB_WARPER, JOB_HEXSLINGER,
          JOB_ENCHANTER, JOB_REAVER }
}
~~~

For UI consistency, it is also relevant to note that species and background
recommendations are not automatically bi-directional. If you select your
background first, you will see appropriate species recommendations as defined in
**job-data.h**. However, if you select species first, you will not see those
same recommendations. When starting from species selection, recommendations are
derived from the species yaml files, found in **source/dat/species**. To make
the menu recommendations fully symmetrical, change each species yaml file that
you identified in your job-data.h struct. This example is the relevant section
in human.yaml:

~~~ recommended_jobs:
  - JOB_BERSERKER
  - JOB_CINDER_ACOLYTE
  - JOB_CONJURER
  - JOB_NECROMANCER
  - JOB_ICE_ELEMENTALIST
  - JOB_CUSTOM
~~~

### Final Items

And we're done. Compile and enjoy your new background!

... Unless your background has some other, more complicated starting situation
the way that Abyssal Knights and Chaos Knights do. Those can be set in
**ng-setup.cc**. If you have some limitations or restrictions on your job (like
locking out a species for some reason, or if a piece of gear would be limited by
some racial element), you can add those checks into **ng-restr.cc**. Finally,
the only other job-related special case is the Monk, which gets its piety bonus
handled in **religion.cc**.

Wanderers are special, and get their own file. There are seven total files that
may be required to fully define a background, but in most cases you should only
have to edit three.

## Summary and Checklist

  - **job-type.h** to add the background to the list
  - **job-data.h** to fill in detailed info about the job.
  - **newgame.cc** to add it to the selection menu.
  - **ng-setup.cc** for any factors you can't set in the job struct.  (and/or
    **religion.cc** if you're messing with piety after character creation like
the Monk)
  - **ng-restr.cc** for limitations to your background.
  - **ng-wanderer.cc** for Wanderer nonsense.
  - **source/dat/species** yaml files for bi-directional recommendations

## Version history

Written by Cerol, 07 FEB 2017  Updates by Sam Willett, 14 NOV 2023
