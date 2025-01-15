++++++++++++++++++++++++++++++++++++++++
Dungeon Crawl Stone Soup manual
++++++++++++++++++++++++++++++++++++++++

.. contents::
   :depth: 5

########################################
Manual
########################################

****************************************
A. Overview
****************************************

Crawl is a fun game in the grand tradition of similar games like Rogue, Hack and
Moria. The objective is to travel deep into a subterranean cave complex and
retrieve the Orb of Zot, guarded by many horrible and hideous creatures.

If you have never played Crawl (or a similar game) before, select the tutorial
from the starting menu. The tutorial explains the interface in five easy
lessons. Once you're familiar with the controls, you may want to play a few
games using hints mode.

Detailed instructions for playing Crawl follow. To simplify this manual, we
assume you're using the standard distribution and you've not changed the default
options. If you don't want to read the whole manual and prefer a short summary
of the important points, review the quick-start guide file (quickstart.txt) and
learn as you play.

You can also read these documents while playing Crawl by hitting '?' at the help
menu. A list of frequently asked questions about gameplay and design can be
accessed by pressing '?Q' in the game.

While Crawl is strictly a single player game, you can interact with others over
a server. Connecting to a server opens several interesting options.

You can:

- watch other players and even communicate with them
- view your past games or those of others
- battle ghosts of other players
- compete using a common score list
- take part in the semiannual tournament
- play the most recent development version

See the Crawl website to find an online server, links to download the game for
offline play, and other community resources:

  http://crawl.develz.org/

****************************************
B. Starting screen
****************************************

At the Crawl start screen, you will be presented with various game modes, a list
of saved games (if any), and will be prompted to type a name for your character.
You can navigate the game modes and saved games with the cursor 'up' and 'down'
arrow keys.

Game modes are:

Dungeon Crawl
  Start a standard game of Crawl.

Choose game seed
  Start a standard game of Crawl with a custom seed (see `Seeded play`_ below).

Tutorial for Dungeon Crawl
  Start one of several specialised tutorials to learn how to play.

Hints Mode for Dungeon Crawl
  Start a standard game of Crawl, modified to provide help as you play.

Dungeon Sprint
  Start one of several single-map challenge mode games of Crawl.

Instructions
  View the instructional help text you are currently reading.

The Arena
  Initiate an automated fight mode between two specified combatant groups.

High Scores
  View scores for prior games played, sorted by decreasing score.

Pressing 'enter' immediately after typing your name will select standard Dungeon
Crawl mode, and you will be prompted to select a species and background. Names
are unique; entering a name from the saved game list will continue that saved
game. If you select a game mode instead of entering a name, you will eventually
be prompted to enter a name.

You can choose species and background in either order or randomise any
combination if you would rather have the game decide for you. If both are
random, you will be prompted to confirm the chosen combination before the game
starts.

The choice of species affects several important characteristics, in particular
the speed at which you learn different skills. This is very important, and helps
to clearly differentiate the many available species. The following factors are
species-dependent:

Major:

- The amount of health you get each level
- Your rate of skill advancement
- Your initial primary attributes (this also depends on background)
- Various special abilities, powers and limitations

Minor:

- Your rate of level advancement
- Occasional bonus points added to some primary attributes
- The amount of magic points you get each level
- Your innate willpower, your resistance to subtle magic
- Your initial equipment (this also depends on background)

.. note:: Humans are the average to which all other species are compared.

The choice of character background is definitely less decisive than that of
species in Crawl. Basically, the background determines what the character has
learned prior to entering the dungeon (i.e. the starting skills), and also helps
determine equipment at start.

You will notice that a different set of backgrounds will be recommended (white)
for each species. Although you are free to pick any background with almost any
species (the only restrictions are religious backgrounds where some species
are not permitted to worship certain gods, or backgrounds where the starting
equipment is completely unusable by a given species), looking at the recommended
combinations should give you a rough impression of the weaknesses, strengths,
and roleplaying flavour of the different species.

For some backgrounds, you must pick a starting weapon before starting the game.

When you start a new character (or load an old one) and want to get a rough
impression, you may read the full character dump with ?# or examine it with the
following commands:

A
  shows any peculiarities like unusual speed or body parts

m
  shows your skills and lets you disable (or focus) training as desired

i
  lists equipment and items

^
  displays information on your god, should you have started with a religion

%
  gives a general, if terse, overview of your gear and most attributes

Ctrl-O
  gives an overview of the parts of the dungeon you have discovered so far

****************************************
C. Attributes and stats
****************************************

The stat area to the right of the playing map shows a lot of information. It
starts with the character's name and title (determined by the character's
highest skill), followed by a line listing the species. If the character
worships a god, the name of the deity is also listed in the second line,
together with an indicator of piety. Below these two lines follow a lot of
numbers. These describe different aspects of the character.

Health
  A measure of life force. Synonymous with hit points and sometimes abbreviated
  as HP. You die if your health drops to zero or less (although you can die in
  other ways, too). The main screen shows both your current and maximum health.
  Usually, you regain health slowly over time. Pressing '5' or Shift-Num-5 lets
  you wait for a longer period.

Magic
  A measure of magic or other intrinsic power. This is used primarily for
  spellcasting, but is sometimes also used for the evoking and invoking of many
  other special abilities. They are displayed in the same way as health;
  nothing bad happens if these drop to zero, except, of course, that you can't
  cast any spells. Resting also restores your reserves of magic.

Next come your defences. For all of them, more is better.

Armour Class
  Abbreviated to "AC". Your AC reduces the amount of damage you suffer from
  most attacks (with a few rare exceptions), and provides some guaranteed
  minimum protection against damage from melee attacks.

Evasion
  Abbreviated to "EV". This helps you avoid being hit by unpleasant things (but
  will not reduce the amount of damage you suffer if you do get hit).

Shield
  Abbreviated to "SH". This number is a measure of how good your shield (if any)
  is at blocking attacks.

Your character's primary attributes are Strength, Intelligence and Dexterity:

Strength
  Abbreviated to "Str". Increases your damage with melee weapons (except for long
  and short blades), with unarmed combat, and with throwing weapons. Reduces
  penalties from wearing shields or heavy armour.

Intelligence
  Abbreviated to "Int". Affects your odds of successfully casting spells and how
  powerful those spells are.

Dexterity
  Abbreviated to "Dex". Increases your accuracy with melee and ranged weapons,
  and your damage with ranged weapons and long and short blades. Significantly
  affects your ability to dodge attacks aimed at you, your effectiveness with
  shields, your stealth, and your effectiveness when stabbing unaware enemies.

These primary attributes grow permanently from gaining levels, and may
increase or decrease temporarily from mutations or while using certain
artefacts or abilities. Upon gaining levels 3, 9, 15, etc., you may choose an
attribute to raise by two points. Most species gain additional attributes at
some levels, with the frequency and the attribute to be increased determined by
species.

If any attribute drops to zero for some reason, you will experience very
unpleasant side-effects, being slowed and suffering some stat-specific
negative effects. These effects will persist for a short while even after the
attribute is restored.

Finally some additional information about your character and your progress
through the dungeon is displayed.

Experience Level
  Abbreviated to "XL". Starting characters have experience level 1; the highest
  possible level is 27. Gaining a level nets additional health and magic points,
  and will grant spell slots and sometimes primary attributes.

Place
  This shows the branch you are currently in, as well as the level within the
  branch. The starting branch is called Dungeon, so that the place information
  will read "Dungeon:1" for a new character.

Noise
  This is a coloured bar indicating the loudness of noise that you heard on your
  last turn. The colour provides a rough guide to how far away the noise it
  indicates might be audible. If the bar is gray, the sound is less likely to
  be audible outside of your line of sight (at least in an open area); if it is
  yellow, the sound is likely to be audible outside of your line of sight; and
  if it is red, the sound will be audible at a substantial distance. If the bar
  turns magenta, you have made one of the loudest noises in the dungeon. N.b.:
  terrain can reduce or block the spread of noise.

Time
  This indicates the amount of time that has passed since entering the dungeon,
  and also displays in brackets the amount of time that your previous action took.
  Most actions take 1.0 units of time, but certain actions are particularly slow
  or quick (such as changing armour and swapping weapons respectively), and other
  actions can vary in time taken depending on your status (such as attacking with
  different weapons and at different skill levels).

There are some additional stats that aren't as important on a turn to turn basis
and thus aren't listed in the main stats area. They can easily be checked with
the '@' or '%' commands, though.

Willpower
  Affects your ability to resist the effects of enchantments and similar
  magic directed at you. Has no effect on direct damage dealt by magic,
  just on more subtle effects. Although your willpower increases with your
  level to an extent determined by your character's species, the creatures
  you will meet deeper in the dungeon are better at casting spells, and are
  more likely to be able to affect you. You can get a rough idea of your
  current Will by pressing '%'.

Size
  Different species have different sizes: Spriggans and Felids are very small;
  Kobolds are small; Oni and Trolls are large; Nagas and Armataurs are large
  with a medium torso; all other species are medium-sized. Many talismans will
  change your size. Size affects your evasion: the smaller your character, the
  more evasive it is. On the other hand, characters of larger than medium size
  do not suffer the usual attack penalties when standing in shallow water.
  Characters of smaller than medium size will have problems with some larger
  weapons. Very small characters and large characters are not able to use most
  types of armour. Players and monsters can only constrict foes of the same size
  or smaller. In the dungeon you can also meet tiny and giant foes.

Stealth
  High stealth allows your character to move through the dungeon undetected.
  It is affected by your species, dexterity, Stealth skill, and the encumbrance
  of your body armour. Your current Stealth level can also been seen by pressing
  '@' or '%'.

There are many ailments or enchantments that can temporarily befall you. These
are noted in the stats area below the experience line. Many of them are
self-explanatory, like Pois or Slow. Many others, however, can be subtle, and
their effects can be examined by pressing '?/T' and searching for the name of
status effect.

Monsters within your field of vision are listed on a special panel, the monster
list. Single monsters also get indicators of their health status in the form of
a coloured box, and also on effects they enjoy or suffer from. If there are
many monsters in view, the extended monster list can be displayed with
'Ctrl-X'. Within target mode you can directly target single monsters by use of
the monster list by using 'Ctrl-X'.

Sometimes characters will be able to use special abilities, e.g. the Naga's
ability to spit poison or the magical power to turn invisible granted by a
scarf of invisibility. These are accessed through the 'a' command.

****************************************
D. Exploring the dungeon
****************************************

Movement
========================================

You can make your character walk around with the numeric keypad (try both
NumLock on and off) or the "Rogue" keys (hjklyubn). If this is too slow, you can
make your character walk repeatedly by pressing Shift and a direction;
alternatively, press '/' followed by a direction. You will walk in that
direction until any of a number of things happen: a hostile monster is visible
on the screen, a message is sent to the message window for any reason, you press
a key, or you are about to step on anything other than normal floor and it is
not your first move of the long walk. Note that this is functionally equivalent
to just pressing the direction key several times.

Another convenient method for moving long distances is described in the section
on Automated Travel and Exploration below.

Combat
========================================

The dungeon is a hostile place, and you will likely need to defend yourself. The
basic case of combat involves melee attacks: if you are adjacent to a monster,
moving towards that monster will cause you to attack it with a wielded melee
weapon, or your fists. There are also a wide variety of ways to attack monsters
that are further away, including polearms (which can reach a tile away), various
bows, launchers, and throwable items, magical items that can be evoked such as
wands, spells, and abilities. These are described throughout the rest of this
document. These are triggered in a variety of ways, but one commonality is that
all of these can be quivered: the quiver provides quick access to an action that
can be fired.

Both melee and ranged combat provide shortcuts that autotarget the nearest
enemy, if there is one available. Your wielded weapon can be triggered by Tab
("Autofight"), moving towards or attacking the nearest enemy depending on
whether there is one in range for the weapon. Your quivered action can be
triggered by Shift-Tab or 'p' ("Autofire"); if the action takes a target the
nearest enemy will be selected, otherwise the action will be triggered. See
`Attacking and firing`_ for the full key list related to attacking, quivers, and
firing.

Resting
========================================

If you press '5', you will rest until your health or magic return to full. You
can rest for just one turn by pressing '.' or 's'.

Resting stops if a monster appears or if you are otherwise interrupted.

Examining your surroundings
========================================

The section of the viewing window which is coloured (with the "@" representing
you at the centre) is what you can see around you. The dark grey around it is
the parts of the level which you have visited, but cannot currently see. The 'x'
command lets you move the cursor around to get a description of the various
dungeon features, and typing 'v' when the cursor is over a monster or feature
brings up a short description of that monster, as well as a short list of its
various strengths, weaknesses, immunities, and any spells or abilities it has.
This is generally useful with monsters you've never encountered before. You can
also select monsters and features from a list by pressing 'Ctrl-X'. You can get
a map of the whole level (which shows where you've already been) by typing 'X'.

You can see the full set of commands available while looking around by pressing
'?', both in the 'x' and 'X' modes.

Staircases and Portals
========================================

You can make your way between levels by using staircases, which appear as ">"
(down) and "<" (up), by pressing the '>' or '<' keys. It is important to know
that most monsters adjacent to you will follow when you change levels; this
holds both for hostile and allied monsters. Notable exceptions are zombies
and other mindless undead, who are too stupid to properly operate stairs.

When taking stairs with an unknown destination, you are guaranteed a chance to
move before any adjacent monsters. When taking stairs that you've already seen
the other side of, monsters will be alert and get a chance to move before you.

If you ascend an up staircase on level one, you will leave the dungeon forever;
if you are carrying the magical Orb of Zot, you win the game by doing this.
Otherwise, the game ends, and you lose.

Besides the dungeon you start in, Crawl's dungeon has many branches. All of them
are themed and host opponents of some special sort. It is not mandatory to visit
any particular branch, but you must explore some of them: progress to the Realms
of Zot (where the Orb is located) is only possible for adventurers who have at
least three magical runes of Zot. The bottoms of several branches contain such
runes.

Occasionally you will find an archway (displayed as "\\" or as an actual arch);
these lead to special places like shops, themed one-off portals, or special
branches such as Hell and Pandemonium. You can enter these by typing '<' or '>'.
A few portals only accept one of '<' and '>'.

Doors and Traps
========================================

Doors can usually be opened by just walking into them (there is an option to
disable this); else this can also be done using the 'O' command. They can be
closed with the 'C' command. Pressing Ctrl plus a direction, or '*' followed by
a direction, will open/close doors, too.

If there is no door in the indicated space, both Ctrl-direction and
'*'-direction will attempt to attack any monster which may be standing there
(this is the only way to attack a friendly creature with melee combat). If there
is apparently nothing there, you will still attack it, just in case there's
something invisible lurking around.

The malevolent forces of Zot will attempt to thwart your progress through the
dungeon, and will occasionally manifest traps to force you into dangerous
situations. Some traps are visible in advance and can be triggered by monsters
to unleash their effects on you.

Shops
========================================

When you visit a shop (by using < or > while standing on one), you are shown
what the shopkeeper has in stock and can choose what to buy. Unfortunately,
the shopkeepers all have an exclusive deal with the Guild of Dungeon Procurers
which prevents them using non-guild labour to obtain stock, so you can't sell
anything in a shop. (But then, what shopkeeper would buy stolen goods from a
disreputable adventurer, anyway?)

To purchase one or more items, select them by pressing the letters of the item
in the shop menu and then press Enter to make the purchase; you can examine
items before buying them by pressing '!' and then the letter of the item.

If you've lost track of the shops in the dungeon, you can get a list of all the
shops you've found in the dungeon overview (use 'Ctrl-O').

You can also use the stash search: Hitting 'Ctrl-F' and searching for "shop"
will list all stores. The stash-search menu allows you travel quickly to a
particular shop; if you just want to know what's in the shop, you can also
examine the shop's inventory from the search menu without having to travel all
the way to the shop.

Some shops are antique stores that sell items of unknown provenance, usually at
a good discount. The dungeon overview screen displays these with yellow glyphs.

If you're short on gold for some particularly interesting commodity, you can
place it onto your shopping list. The game will interrupt you when you have
collected enough gold to finally purchase an item on that list. You can read the
shopping list in the game with '$'.

Automated Travel and Exploration
========================================

Crawl has an extensive automated travel system: pressing 'G' (or also 'Ctrl-G')
lets you choose any dungeon level; the game will then take the shortest path to
reach this destination. You can also use autotravel on the level map ('X'): Move
the cursor to the place where you want to go and hit Enter. There are several
shortcuts when choosing destinations: For example, try '<' and '>' to quickly
reach the staircases.

When your autotravel gets interrupted, Crawl will remember the previous
destination. Hitting 'G' or 'Ctrl-G' again and following with Enter puts the
cursor on that square. See Appendix `4. List of Keys and Commands`_ for all
commands and shortcuts in level-map mode, or press 'G?' or 'X?' within the game.

Another use of autotravel is exploration: 'o' makes your character move to the
nearest unexplored area. Note that this algorithm does not attempt any
optimisation by default. By manual exploration you can save turns, but
auto-explore will usually save real time.

Stashes and Searching
========================================

Since you can only carry 52 items, you will occasionally want to stash things
away (by dropping them with the 'd' command). When you want to search for
something in your stashes, you can do this with the Find command 'Ctrl-F'. The
parser even accepts regular expressions, although you will mostly just need
strings like 'mutation', 'heal wounds', etc. You will be presented with a list
of all places where objects matching the search are (or have been) located; you
can then travel there or examine the pile. The Find command will also search in
shop inventories. Furthermore, you can search skills like 'long blades' (this
will find all weapons training the long blades skill) or general terms like
'shop', 'altar', 'portal', 'artefact', etc. You can get help on finding by
pressing '?' at the prompt.

The Goal
========================================

Your goal is to locate the Orb of Zot, which is held somewhere deep beneath the
world's surface. The Orb is an ancient and incredibly powerful artefact, and the
legends promise great things for anyone brave enough to extract it from the
fearsome Dungeon. Some say it will grant immortality or even godhood to the one
who carries it into the sunlight; many undead creatures seek it in the hope that
it will restore them to life. But then, some people will believe anything. Good
luck!

Zot
========================================

No one knows who or what Zot is: demon, god, wizard, something else entirely?
But adventurers seeking the Orb will, in turn, be hunted by Zot. Even a moment's
contact with Zot is enough to permanently damage one's health.

Zot is very slow moving, and can only sense adventurers once they've spent an
inordinately long time in one area of the dungeon. Even once this happens,
adventurers will have plenty of warning to either descend to new depths in the
area (diffusing Zot's attention once more) or to leave the area entirely.

The Abyss is beyond Zot's comprehension, and adventurers there are safe from
it (albeit subject to many other hazards). The Hells, along with their
Vestibule, are a single area as far as Zot is concerned. Rumours claim there
are ways to escape Zot's pursuit forever, but what magics could be mighty
enough to forestall such an implacable entity?

Seeded play
========================================

Crawl dungeons are determined by a "seed" number used to initialise the game's
random number generator. You may either let the game choose a seed randomly,
or specify a seed; if you choose a seed this puts the game in "Seeded" mode,
which is scored separately. Playing games with the same seed value, as long as
the game version is constant, should (within certain parameters) lead to the
same dungeon. The entire connected dungeon will be determined by the game
seed, including dungeon layout, monster placement, and items. Portal vaults
and chaotic zones such as the Abyss are not guaranteed to be the same, and the
placement of rare unique artefacts may vary depending on certain player
actions.

To set a game seed, use the "Choose game seed" option from the main menu; you
can also use the 'game_seed' rc file option, or the '-seed' command line
option. In offline games you can view your game's seed with '?V' as well as in
a character file; in online games a randomly chosen seed will only be shown to
you after finishing the game.

If you find that the same seed generates distinct parts of a dungeon on the
same or different devices, please report it as a bug. However, keep in mind
that upgrading your save game between multiple versions of crawl will
naturally lead to seed divergence. When playing offline, if you would like to
ensure that your game can be upgraded without divergence, you can set
'pregen_dungeon = full' in your options file. (This will also ensure
completely stable unique artefact placement.) On the other hand, to completely
disable incremental pregeneration, you can set 'pregen_dungeon = false'.

Further Help
========================================

To access Crawl's help menu, press '?'. To get the list of all commands, press
'?' again. A different, more verbose description of the commands also appears in
Appendix `4. List of Keys and Commands`_ of this text. Various other help texts
are available as well, including this manual. You can also read the logbook from
there by pressing ':'. Note that several commands have their own help screens,
among them are targeting ('f'), level map ('X'), travel ('G' or 'Ctrl-G') and
searching ('Ctrl-F'); again, press '?' when asked for input.

If you don't like the standard keyset (either because some keys do not work
properly, or because you want to decrease the amount of typing necessary), you
can use keymaps and macros. See macros_guide.txt in the docs/ directory, or read
it from the in-game help menu.

****************************************
E. Experience and skills
****************************************

When you kill monsters, you gain experience points (XP). When you get enough XP,
you gain an experience level, making your character more powerful. As they gain
levels, characters gain more health, magic, and spell levels.

Additionally, the experience you gain is used to train your skills. These skills
represent proficiency with all areas of endeavour an ambitious adventurer might
need in the dungeons. They range from different weapon skills (both for close
and ranged combat) to many magical skills and several additional activities like
Dodging or Stealth. See Appendix `3. List of Skills`_ for a detailed
description of all skills present in Crawl. The ease with which a character
learns a skill depends solely on species. These aptitudes are displayed when
viewing your skills, and a full table can be viewed in aptitudes.txt (also from
the help screen during play via '?%').

You can see your character's skills by pressing the 'm' key; the higher the
level of a skill, the better you are at it. All characters start with a few
skills already trained (determined by their background), those which are not
present have to be learned from scratch. Each skill can go up to 27.

The skill screen allows you to change which skills are exercised and at what
speed. Note to new players: it is generally not necessary to finetune the skill
selection.

If you want to modify skill selection, here is how:

There are two ways to assigning skills to practise: one is an automatic mode,
which puts experience points into the skills you have used recently. The other
one is a manual mode, where you specifically select the skills to train. You can
switch between the modes by pressing '/' in the character skill screen ('m');
also be sure to read that screen's help text should you want to tweak your
skillset.

You can elect to either not practise a particular skill or to focus on it by
selecting it once or twice in the skill screen.

Dark grey skills will not be trained, so that the skill will remain static and
no experience points will be used to increase it. As a consequence, more
experience will be spent on your other skills (and thus they will increase more
quickly). Note that you cannot deselect all skills; at least one skill must be
actively exercised.

Highlighted skills are focused on and will use a higher proportion of available
experience. You can highlight as many skills as you like, though obviously if
all skills being trained are highlighted there will be no net effect.

Occasionally you may find a manual of a skill which allows you to make quick
progress in this area. When you pick it up, experience used to practise the
given skill will have twice the usual effect for a while.

****************************************
F. Monsters
****************************************

In the caverns of Crawl, you will find a great variety of creatures, most of
which are displayed by capital or small letters of the alphabet. Many of them
would very much like to eat you. To stop them from doing this, you will
generally need to fight them. To attack a monster, stand next to it and move in
its direction; this makes you attack it with your wielded weapon. Of course,
some monsters are just too nasty to beat, and you will find that discretion is
often the better part of valour. Sneaky characters are known to choose
encounters to their liking.

There are several other ways to kill monsters. When using a bow or other ranged
weapon, the 'v' command will fire. See the section on Targeting in the Items
Chapter for more on this. Likewise, many magicians will prefer to use spells
from a safe distance. They can use the 'z' command to cast spells previously
'M'emorised. Again, see the Targeting section.

Some monsters can be friendly; friendly monsters will follow you around and
fight on your behalf. You can command your allies using the 't' key, which lets
you tell them who to attack, or else tell them to stay where they are, retreat,
or to follow you again. You can also shout to get the attention of all monsters
in range if, for some reason, you want to do that.

Some special monsters are Uniques. You can identify a unique because he, she or
they have a name and personality. Many of these come up with very nasty ideas
how to rid the dungeon of you. Treat them very carefully, particularly if you
meet one of them for the first time.

Other, even rarer, obstacles are statues. A variety of statues can appear,
ranging from harmless granite ones (who still often signify something of
interest) to really dreadful ones. Be alert whenever seeing such a statue.

When playing Crawl, you will undoubtedly want to develop a feeling for the
different monster types. For example, some monsters move unpredictably while
most attack head-on. Likewise, ranged or magic attackers will prove a different
kind of threat from melee fighters. Learn from past deaths and remember which
monsters pose the most problems. If particular monsters are giving you
trouble, try to alter your tactics for future encounters.

You can obtain information about a monster by using the 'x' (examine) command,
moving the cursor over the monster in question, and pressing 'v' to view the
monster's details; or by searching for a monster by name or symbol with '?/m'.
The details screen shows:

- The monster's name and description.

- Bars or numbers indicating its:

  * Max HP: hit points; how much damage it can take

  * AC: armour class; how well it ignores most damage

  * EV: evasion; how well it avoids being hit (and your odds of hitting it
    with your current melee attack)

  * Will: willpower; its resistance to most Hexes and similar effects.

- Its difficulty level, speed (if different from average speed), size,
  resistances, and special attacks.

- Its spells and special abilities. Monster spells and abilities are
  of four types:

  * Natural abilities are innate, non-magical effects.

  * Magical abilities are innate magical effects. They are affected
    by antimagic.

  * Divine abilities call upon the monster's god for a magical effect.
    They are prevented by silence, but not affected by antimagic.

  * Spells are cast using memorised magical words. They are both
    prevented by silence and affected by antimagic.

****************************************
G. Items
****************************************

In the dungeons of Crawl there are many different kinds of normal and magical
items to be found and used. Some of them are useful, some are nasty, and
some give great power, but at a price. Some items are unique; these have
interesting properties which can make your life rather bizarre for a while. They
all fall into several classes of items, each of which is used in a different
way. Here is a general list of what you might find in the course of your
adventures, how they are displayed, and what commands there are to use them:

=======  =============  ================================================
)        weapons        (use 'w'ield)
(        missiles       (use 'f'ire or 'F'ire, 'Q' to quiver)
[        armour         (use 'W'ear and 'T'ake off)
?        scrolls        (use 'r'ead)
!        potions        (use 'q'uaff)
/        wands          (use 'V' to evoke, 'Q' to quiver)
=        rings          (use 'P'ut on and 'R'emove)
"        amulets        (use 'P'ut on and 'R'emove)
\|       staves         (use 'w'ield)
:        spellbooks     (use 'M'emorise and 'z'ap, 'Q' to quiver)
%        talismans      (use 'V' to evoke)
}        miscellaneous  (use 'V' to evoke, 'Q' to quiver)
$        gold           (use 'g' to pick up)
=======  =============  ================================================

There are several general keys for item management:

d
  drop item; if you want to drop only some items from a stack (of arrows, for
  example), then press 'd' followed by a number and then the item's slot key

g
  pick up item from the ground (also with the comma key ',')

=
  reassign item slot (works also for spell slots and abilities)

i
  show inventory - pressing the key of an item shows additional information

{
  inscribe item (see Appendix `5. Inscriptions`_)

\\
  check list of already discovered items

Item usage
========================================

You pick up items with the 'g'et or ',' (comma) command, and drop them with the
'd'rop command. When you are given a prompt like "drop which item?", if you type
a number before the letter of the item, you will drop that quantity of the item
(similarly when picking up). The same works if you want to pick up only part of
a stack and there are several types of items on the square (so that they are
shown in a list). When there is only a single stack of arrows and you want to
pick up only some of them, use the ';' command. Note that picking up items from
one square takes exactly one turn. However, dropping several items at once takes
more turns.

Typing 'i' displays your inventory (what you are carrying). When you are given a
prompt like "Throw [or Wield, Wear, etc] which item?", you usually get a list of
all available options. You can press '*' in case you want to wield something
unusual. When the inventory screen shows "-more-", to show you that there is
another page of items, you can type the letter of the item you want, even if it
is not visible, instead of pressing Space or Enter to see the next page.

You can carry at most 52 items at once, and your item slot usage is printed at
the top of the inventory screen.

You can use the adjust command (the '=' key) to change the letters to which your
possessions are assigned. This command can be used to change spell or ability
letters, too.

Items like scrolls, potions, and some other types each have a characteristic,
like a label or a colour, which will let you tell them apart on the basis of
their function. However, these characteristics change between each game, so
while in one game every potion of curing may be yellow, in another game they
might all be purple and bubbly. Once you have discovered the function of such an
item, you will remember it for the rest of the current game. You can access your
item discoveries with the '\\' key.

In order to get a description of what an item does, bring up the inventory (with
'i') and press the letter of that item. Try this when comparing different types
of armours and weapons, but don't expect too much information from examining
unidentified items.

In most equipment-related prompts and menus, the ';' key is a shortcut for
"last unequipped item," meaning the armour, jewellery or weapon you most
recently took off or unwielded.

Another useful command is the '{' key, which lets you inscribe items with a
comment. You can also inscribe items when looking at your inventory with 'i',
simply by pressing the letter of an item. For more details, and how to automate
this process, check Appendix `5. Inscriptions`_.

) Weapons
========================================

These are rather important. You will find a variety of weapons in the dungeon,
ranging from small and quick daggers to huge, cumbersome battleaxes and
polearms. Each type of weapon does a different amount of damage, has a different
chance of hitting its target, and takes a different amount of time to swing.
There are several weapon skills (press 'm' to show a list of those that you are
training) like Short Blades, Long Blades, Axes, etc. These skills affect damage,
accuracy, and speed (up to a point). The same goes for Unarmed Combat.

Weapons can be enchanted; when you first identify them, you reveal values which
tell you how much more effective they are than an unenchanted version. Weapons
which are not enchanted are simply '+0'. Some weapons also have special magical
effects which make them very effective against vulnerable enemies.

You can wield weapons with the 'w' command, which is a very quick action. If for
some reason you want to go bare-handed, type 'w' followed by a hyphen ('-').

The ' (apostrophe) key is a shortcut which automatically wields the item in slot
a. If item a is being wielded, ' causes you to wield item b instead, if
possible. Try assigning the letter a to your primary weapon, and b to your bow
or something else you need to wield only sometimes. Note that this is just a
typing shortcut and is not functionally different to wielding these items
normally.

( Missiles
========================================

If monsters are disobligingly distant, you can use missiles to weaken (or kill!)
them from afar. You'll find a variety of type, ranging from simple stones and
piercing javelins to sophisticated darts covered in many types of poisons. Upon
impact, missiles may become destroyed. The chance for this to occur depends on
the type of missile.

The 'F' and 'f' commands can be used to throw a missile. The default type to be
thrown (which 'f' will launch) is shown in the "quiver" display below your
weapon. Many other items, spells, and abilities can fill this quiver as well.

See Appendix `5. Inscriptions`_ for inscriptions which let you fine-tune the
list of items to choose from. See also the Missiles section of
options_guide.txt.

Use the '(', ')' to cycle through your quiver without firing, and 'Q' to choose
a quivered item from a list. If you would like to choose something to fire
without inserting it into the quiver, use 'F' instead.

The interface for shooting or throwing things is also used for evoking wands and
casting certain spells, and is described in detail in section I (Targeting).

[ Armour
========================================

This is also rather important. Most worn armour improves your Armour Class,
which decreases the amount of damage you take from most types of injury. The
heavier an armour is, the more AC (armour class) it will provide, at the expense
of your EV (evasion) and stealth. Wearing heavy armour also increases your
chances of miscasting spells and slow your attacks with missile weapons, effects
which are only slightly reduced by your Armour skill. These penalties are larger
if you have low Strength.

A shield normally increases neither your AC nor your evasion, but it lets you
attempt to block melee attacks and some ranged attacks aimed at you. Wearing a
shield (especially larger shields) slows your attacks, hampers your ability to
cast spells, and lowers your evasion. Weaker characters are more affected by
these penalties, but all characters can reduce and eventually eliminate these
penalties by mastering the Shields skill. You also obviously cannot wield a
two-handed weapon while wearing a shield. Shields are limited in how many
attacks they can block each turn; larger shields can block more.

Some magical armours have special powers. These powers are sometimes automatic,
affecting you whenever you wear the armour, and sometimes must be activated with
the 'a' command.

You can wear armour with the 'W' command, and take it off with the 'T' command.
With '[' you can have a quick look at your current gear.

Most armours can be improved by reading the appropriate scroll. Body armour and
bardings can be enchanted up to the base value of AC they provide. Shields can
be enchanted up to +3, +5, or +8, depending on their size. Other gear is limited
to +2.

? Magical Scrolls
========================================

Scrolls have many different magical spells inscribed on them, some good and some
bad. One of the most useful scrolls is the scroll of identify, which will tell
you the function of any item you have in your inventory; you might want to save
these up for items that are dangerous or wasteful to use when unidentified, such
as potions or other scrolls. You can read scrolls (and by doing so invoke their
magic) with the 'r' command.

! Magical Potions
========================================

While scrolls tend to affect your equipment or your environment, most potions
affect your character in some way. The most common type is the simple curing
potion, which restores some health and cures many ailments, but there are
many other varieties of potions to be found. Potions can be quaffed (drunk) with
the 'q' command.

/ Wands
========================================

Sometimes you will be lucky enough to find a stick which contains stored magical
energies. Wands each have a certain number of charges, which you immediately
recognise when you pick them up. When you pick up a wand of type you already
have in inventory, its charges are absorbed into the existing one. When a wand's
charges are fully depleted, it vanishes.

Wands are aimed in the same way as missile weapons, and you can release the
power of a wand by evoking it with 'V'. See section I for targeting.

Wands can be 'Q'uivered in order to shoot via the autofire or 'f'ire
interface, like spells and ammo.

=" Rings and Amulets
========================================

Magical rings are among the most useful of the items you will find in the
dungeon. While equipped, they provide some kind of passive benefit to the
wearer, such as increasing their attributes or providing various types of
protection. Use the 'P' command to put on rings, and 'R' to remove them. You can
wear up to two rings simultaneously, one on each hand; which hand you put a ring
on is immaterial to its function. If you try to put on a ring while both ring
fingers are full, you will be asked which one to remove. Octopodes are an
exception, and may wear up to eight rings on their tentacles.

Amulets are similar to rings, but have different range of effects. Amulets are
worn around the neck, and you can wear only one at a time. It is very quick to
wear or remove a ring, but amulets' magics make them cumbersome to put on or
take off.

You can press '"' to quickly check what jewellery you're wearing.

\| Staves
========================================

There are a number of types of magical staves, each attuned to a different
class of spells. While wielded they greatly increase the power of that class
of spells. They can even be used in melee combat, although with mediocre
effectiveness unless you can harness their special power, using a combination of
the Evocations skill and the skill specific to the staff's type.

: Books
========================================

Most books contain magical spells which your character may be able to learn.
Upon picking up a book, all of the spells in it will be added to your spell
library, allowing you to access a description of each spell or memorise spells
from it with the 'M' command.

Occasionally you will find manuals of some skill. When you pick one up, your
experience will have twice the usual effect when used for training that skill.
Once a certain amount of bonus experience has been gained in this way, you will
automatically discard the finished manual.

% Talismans
========================================

Talismans allow their user to shift into a different form. Entering or leaving
a form with a talisman requires a brief period of concentration, but otherwise,
forms last until the user chooses to leave them.

More powerful talismans require some amount of Shapeshifting skill, without
which a user will find their maximum health reduced until they leave the form.
Shapeshifting skill also increases other benefits provided by talismans' forms,
though weaker talismans have a limit to how helpful skill can be.

{ Miscellaneous
========================================

These are items which don't fall into any other category. They can be evoked
with 'V', just like wands. Runes, a particular item in this category, have no
function whatsoever except to open the endgame. You must collect at least three
in order to enter the Realm of Zot. Some particularly cocky adventurers brag
about having retrieved ten or even fifteen runes through their strength and
cunning, but most scholars on the subject of Zot agree that such a thing is
probably impossible in the first place, and secondly would be a meaningless
achievement in any regard.

Miscellany can often be 'Q'uivered in order to shoot via the autofire or 'f'ire
interface, like spells and ammo.

$ Gold and Gems
========================================

Gold can be used to buy items should you run across shops. There are also a
few more esoteric uses for gold.

Gems are extremely rare items found at the end of many dungeon branches. They
are completely useless within the dungeon, and only a particularly cocky
adventurer would set out to retrieve even one to the outside world. Zot hoards
these gems jealously, and will spitefully smash them soon after an adventurer
enters the branch of the dungeon in which they rest.

Still, a very quick-moving adventurer might seize a gem first and keep its
precious shards. Zot cannot track gems outside their home branches, so with
truly astonishing speed, it might even be possible to abscond with one still
intact... but such a feat is difficult to credit, and likely pointless besides.

Once the Orb of Zot is taken, Zot will be unable to smash any gems.

Artefacts
========================================

Weapons, armour, jewellery and spellbooks can be artefacts. These come in two
flavours: randomly created artefacts ('randarts') and predefined ones
('unrandarts'). Randarts will always carry unusual names, such as "golden
double sword" or "shimmering scale mail". Artefacts cannot be modified in any
way, including enchantments.

Apart from that, otherwise mundane items can get one special property. These are
called 'ego items', and examples are: boots of flight, a weapon of flaming, a
helmet of see invisible, and so on. Note that, unlike artefacts, such items can
be modified by enchanting scrolls.

All ego items are noted with special adjectives but not all items noted in this
way need have a special property (they often have some positive enchantment,
instead):

:general: glowing, runed;
:metal armours: shiny;
:leather armours: dyed;
:other armours: embroidered.

****************************************
H. Spellcasting
****************************************

Magical spells are a very important part of surviving in the dungeon. Every
character can make use of magical spells.

There are many skills related to magic, the principal one being Spellcasting.
Spellcasting determines the number of Magic Points available; it also helps to
cast any spell, though less so than schools associated with a spell. Next, there
are several general magical schools (Conjuration, Hexes, Summoning, Necromancy,
Forgecraft, Translocation and Alchemy) as well as several elemental schools
(Fire, Ice, Air and Earth). A particular spell can belong to up to three
schools. Being skilled in a spell's schools improves the casting chance and the
power of that spell.

Spells are stored in books, which you will occasionally find in the dungeon.
Once you have picked up a book and added its contents to your spell library, you
can memorise a spell using the 'M' command.

In addition to picking up new spells, your character may also wish to get rid of
old ones by reading a scroll of amnesia, which will let you pick a spell to
forget.

Each spell has a level. A spell's level denotes the amount of skill required to
use it, the MP cost of casting it, and indicates how powerful it may be. You
can only memorise a certain number of levels of spells; type 'M' to find out
how many. When you gain experience levels or advance the Spellcasting skill,
your maximum increases; you will need to save up for several levels to memorise
the more powerful spells.

There are two ways to activate memorised spells: by "quivering" them and using
the fire interface, or directly by pressing 'z' (for Zap). To choose a spell
for the quiver, use 'Q', or '(' and ')' to cycle among possible actions. Press
'f' to enter the targeting interface, or shift-tab / 'p' to autofire a
quivered spell at the nearest monster.

Use 'I' to display a list of all memorised spells without actually casting one.
The spells available are labelled with letters; you are free to change this
labelling with the '=' command. You can assign both lowercase and uppercase
letters to spells. Some spells, for example most damage dealing ones, require a
target. See the next section for details on how to target.

Most spells have caps on their effects: no matter how intelligent and proficient
you are, there is a limit to the damage you can achieve with a Magic Dart. In
general, it is a good idea to look at the output of the 'I' and 'II' screens to
get a picture on your casting abilities. This is especially useful if you're
about to change armour or rings.

High level spells are difficult to cast, and you may miscast them every once in
a while (resulting in a waste of magic and possibly dangerous side-effects).
Your chance of failing to cast a spell properly depends on your skills, your
intelligence, the level of the spell and whether you are wearing heavy armour.
The chance of miscasting a spell is displayed on the spell screen, and coloured
based on severity (yellow for moderate damage, light red for major
damage, red for extreme damage, and magenta for potentially lethal damage).

Be careful of magic-using enemies! Some of them can use magic just as well as
you, if not better, and often use it intelligently.

****************************************
I. Targeting
****************************************

When throwing or firing something, evoking wands, or casting certain spells,
you are asked for a direction. There are several ways to tell Crawl which
monster to target.

You can press '?' when asked for a direction; this will bring up a help screen.
Otherwise, you use the following commands:

- The cursor will target on the monster which is closest to your position.
  Should you have been firing at something previously, with the offender still
  being in sight, the cursor will instead rest on the previous target.
- Pressing '+' or '=' moves the cursor to the next monster, going from nearer to
  further away. Similarly, '-' cycles backwards.
- Any direction key moves the cursor by one square. Occasionally, it can be
  useful to target non-inhabited squares.
- Targets can be selected from a list by pressing 'Ctrl-X'.
- When you are content with your choice of target, press one key of Enter, Del,
  or Space to fire at the target. If you press '.', you also fire, but the
  spell/missile will stop at the target's square if it misses. This can be
  useful to keep friendlies out of the fire, or to make sure your precious
  missiles won't end up in deep water.
- You can press Escape if you changed your mind - no turns are deducted.

There are some shortcuts while targeting:

- Typing Shift-direction on your keypad fires straight away in that direction.
- Pressing 'p' or 'f' fires at the previous target (if it is still alive and in
  sight). Due to this, most hunters can go a long way by pressing 'vf' to fire
  their ammunition at a monster and then keep firing at it with further 'vf'
  strokes. At times, it will be useful to switch targets with the '+' or '-'
  commands, though.

If you target yourself while firing something harmful (which can be sensible at
times), you will be asked for confirmation.

Finally, the ':' key allows you to hide the path of your spell/wand/missile.

****************************************
J. Religion
****************************************

There are a number of gods, demons and other assorted powers who will accept
your character's worship, and sometimes give out favours in exchange. You can
use the '^' command to check the requirements of whoever it is that you worship,
and if you find religion to be an inconvenience you can always renounce your
faith (use the 'a' command - but most gods resent being scorned). Further
details can be seen with '!' while in the '^' screen.

To use any powers which your god deems you fit for, access the abilities menu
via the 'a' command; god-given abilities are listed as invocations. Many god
abilities can be 'Q'uivered in order to trigger via the 'f'ire or autofire
interface.

Depending on background, some characters start out religious; others have to
pray at an altar to dedicate themselves to a life of servitude. There are altars
scattered all over the dungeon, and there are rumours of a special temple
somewhere near the surface.

At an altar, you can enter a god's service by pressing < or >. You'll first be
given a description of the god, and then be asked if you really want to join.
To see a list of the standard gods and which of their altars you've seen in your
current game, press 'Ctrl-O'. You can also learn about all gods by pressing
'?/G'.

Note that some gods are picky about who can enter their service; for example,
good gods will not accept demonic or undead devotees.

If you would like to start the game with a religion, choose your background
from Berserker, Chaos Knight, or Cinder Acolyte.

****************************************
K. Mutations
****************************************

The Dungeon contains many sources of mutagenic radiation and magical
contamination, which may cause your character to gain semi-permanent mutations
if affected. You can use the 'A' command to view a list of any mutations that
you have acquired. Individual mutations can be examined in further detail by
pressing the letter they are labelled with.

Many mutations are actually beneficial to your character, but there are plenty
of nasty ones as well. Some mutations have multiple levels, each of which counts
as a single mutation.

Miscasting spells will cause magical contamination, which in turn can cause
mutations if too much contamination is accrued at once. Certain powerful
magical effects or spells (such as 'Invisibility' and 'Irradiate') also cause
contamination as a side-effect even when successful. A single use of these
effects is safe on its own, but multiple uses in short succession, or usage with
existing contamination from other sources can cause dangerous levels of
contamination.

Mutations from magical contamination are almost always harmful. Mutations can
also be caused by specific potions or by spells cast by powerful enemies found
deep in the dungeon.

It is more difficult to get rid of bad mutations than to get one. Using potions
of mutation will remove a number of your current mutations, but will give you
more mutations. These might be better to your taste. However, the only sure-fire
ways is to join the gods Zin or Jiyva, each of whom provides some remedy against
mutations.

Demonspawn are a special case. Characters of this species get certain special
mutations as they gain levels; these are listed in cyan. They are permanent and
can never be removed. If one of your Demonspawn powers has been augmented by a
random mutation, it is displayed in a lighter colour.

Many species have special intrinsic features, like Trolls' claws and Felids' fur.
These can also be viewed on the 'A' screen. If enhanced by a mutation, they will
be displayed in light blue. (Mutations cannot remove species intrinsics.)
Some of these innate features will provide an activated ability, which can be
used with the 'a' command.

Some mutations are only temporary and will dissipate after slaying more enemies.
These are listed in purple on the list of mutations, and marked as temporary.

****************************************
L. Licence, contact, history
****************************************

See licence.txt for information about Crawl's licensing. Most of the game's
components are licensed under version 2 or later of the GNU General Public
License; those that aren't are under compatible licenses.

Disclaimer
  This software is provided as is, with absolutely no warranty express or
  implied. Use of it is at the sole risk of the user. No liability is accepted
  for any damage to the user or to any of the user's possessions.

Contact and community information
========================================

Crawl's homepage is at:

  http://crawl.develz.org

Use this page for direct links to downloads of the most recent version. You can
also submit bug reports on the GitHub issue tracker at:

  https://github.com/crawl/crawl/issues

If you'd like to discuss Crawl, a good place to do so is the #dcss channel
of the Roguelikes Discord:

  https://discord.gg/GtT7xMe

There's also an active subreddit for game discussion:

  https://www.reddit.com/r/dcss/

In both of these communities, topics related to this game usually meet a warm
response, including tales of runes seized, victories (especially first
victories), and sad stories of deceased characters. There are also usually
experienced players around ready to give advice on equipment choices, tight
spots, or other dilemmas. The Discord has a relay to the Sequell IRC bot that
provides helpful information and statistics about the game.

Some players frequent the #crawl channel on the Libera IRC network, which is
also the home of Sequell and other info bots. For those interested in game
development, see the #crawl-dev channel, which is frequented by many members of
the game's development team.

History
========================================

Crawl began as Linley's Dungeon Crawl, created in 1995 by Linley Henzell.
Linley based Crawl on popular roguelikes of the time, namely Moria, Hack, and
NetHack, also taking inspiration from traditional RPGs like Ultima IV. The
object of your quest in Crawl, the Orb of Zot, was taken from Wizard's Castle,
a text adventure written in BASIC.

Linley produced Crawl versions up to 3.30, released in March 1999. Further work
was then carried out by a group of developers who released 3.40 in February
2000. Of these developers, Brent Ross emerged as the single maintainer,
producing versions until 4.0 beta 26 in 2002. Brent released an alpha version
4.1 in August 2005, which vastly overhauled the codebase and reworked many of
the game's aspects, but also considerably increased its difficulty. By this
point, Brent no longer had enough free time to develop Crawl. Hence Darshan
Shaligram, who had previously contributed many UI improvements, recruited
longtime player Erik Piper to start a new project and continue development.

Darshan and Erik aimed to incorporate ideas from the 4.1 alpha and produce a
more balanced an enjoyable game. Calling their project "Dungeon Crawl: Stone
Soup" in reference to their collaborative process, they pulled many 4.1
improvements into the 4.0 beta 26 codebase, play-testing and adjusting the
results. Dungeon Crawl: Stone Soup version 0.1 was released to USENET in
September 2006, with many additional developers subsequently joining the team.
See Darshan's own account of the project's creation here:

  https://crawl.develz.org/wordpress/the-dawn-of-stone-soup

The development of Crawl proceeds to this day, with a team of many developers
and hundreds of contributors.

****************************************
M. Macros, options, performance
****************************************

Crawl supports redefining keys via key maps. This is useful when your keyboard
layout makes some key awkward to use. You can also define macros: these are
command sequences which can make playing a great deal more convenient. Note that
mapping 'a' to some other key will treat almost all pressings of 'a' in that new
way (including dropping and wielding, etc.), so is not recommended. Macroing 'a'
to some other key will only change the command key 'a'.

You can set up key maps and macros in-game with the '~' key ('Ctrl-D' will also
work); this also allows for saving all current key bindings and macros.
Alternatively, you can directly edit the macro.txt file. For more information on
both and for examples, see macros_guide.txt.

Crawl supports a large number of options that allow for great flexibility in the
interface. They are fully documented in the file options_guide.txt. The options
themselves are set in the file ~/.crawlrc (for UNIX systems - copy over init.txt
to ~/.crawlrc) or init.txt (for Windows).

Several interface routines are outsourced to external Lua scripts. The standard
distribution has them in the dat/clua/ directory. Have a look at the single
scripts for short descriptions.

Generally, Crawl should run swiftly on all machines (it compiles out of the box
for Linux, Windows, OS X, and, to some lesser extent, other Unices). If, for
some reason, you find Crawl runs unacceptably slowly on your machine, there are
a few measures which may improve the situation:

  - set travel_delay = -1 to avoid screen redraws during travel (this might be
    especially useful if playing on a remote server)
  - try playing in console mode rather than tiles

****************************************
N. Philosophy (pas de faq)
****************************************

In a nutshell: This game aims to be a tactical fantasy-themed dungeon crawl. We
strive for strategy being a concern, too, and for exquisite gameplay and
interface. However, don't expect plots or quests.

You may ponder about the wisdom of certain design decisions of Crawl. This
section tries to explain some of them. It could also be of interest if you are
used to other roguelikes and want a bit of background on the differences. Prime
mainstays of Crawl development are the following, most of which are explained in
more detail below. Note that many of these date back to Linley's first versions.

Major design goals
  * challenging and random gameplay, with skill making a real difference
  * meaningful decisions (no no-brainers)
  * avoidance of grinding (no scumming)
  * gameplay supporting painless interface and newbie support

Minor design goals
  * clarity (playability without need for spoilers)
  * internal consistency
  * replayability (using branches, species, playing styles and gods)
  * proper use of out of depth monsters

Balance
========================================

The notions of balance, or being imbalanced, are extremely vague. Here is our
definition: Crawl is designed to be a challenging game, and is also renowned for
its randomness. However, this does not mean that wins are an arbitrary matter of
luck: the skill of players will have the largest impact. So, yes, there may be
situations where you are doomed - no action could have saved your life. But
then, from the midgame on, most deaths are not of this type: By this stage,
almost all casualties can be traced back to actual mistakes; if not tactical
ones, then of a strategical type, like wrong skilling (too broad or too narrow),
unwise use of resources (too conservative or too liberal), or wrong decisions
about branch/god/gear.

The possibility of unavoidable deaths is a larger topic in computer games.
Ideally, a game like this would be really challenging and have both random
layout and random course of action, yet still be winnable with perfect play.
This goal seems out of reach. Thus, computer games can be soft in the sense that
optimal play ensures a win. Apart from puzzles, though, this means that the game
is solved from the outset; this is where the lack of a human game-master is
obvious. Alternatively, they can be hard in the sense that unavoidable deaths
can occur. We feel that the latter choice provides much more fun in the long
run.

Crawl has a huge number of handmade vaults/maps to tweak the randomness. While
the placement, and often parts of the contents, of such vaults are random as
well, they provide several advantages: vaults offer challenges that are very
hard to get via just random monster and layout generation; they may centre on
some theme, providing additional immersion; finally, they will often contain
some loot, forcing players to decide between safety and greed.

(The next topic can also be filed under balance; see Replayability for what
balance does not mean to us.)

Crusade against no-brainers
========================================

A very important point in Crawl is steering away from no-brainers. Speaking
about games in general, wherever there's a no-brainer, that means the
development team put a lot of effort into providing a "choice" that's really not
an interesting choice at all. And that's a horrible lost opportunity for fun.
Examples for this are the resistances: there are very few permanent sources,
most involve a choice (like rings or specific armour) or are only semi-permanent
(like mutations). Another example is the absence of clear-cut best items, which
comes from the fact that most artefacts are randomly generated. Furthermore,
scrolls of acquirement offer a random selection of items instead of a specific
wish. Likewise, there are no sure-fire means of life saving (the closest
equivalents are scrolls of blinking, and good religious standings for some
deities).

Anti-grinding
========================================

Another basic design principle is avoidance of grinding (also known as
scumming). These are activities that have low risk, take a lot of time, and
bring some reward. This is bad for a game's design because it encourages players
to bore themselves. Even worse, it may be optimal to do so. We try to avoid
this!

This explains why shops don't buy: otherwise players would hoover the dungeon
for items to sell. Not messing with lighting also falls into this category:
there might be a benefit to mood when players have to carry candles/torches,
but we don't see any gameplay benefit. The deep tactical gameplay Crawl aims
for necessitates permanent dungeon levels. Many a time characters have to choose
between descending or battling. While caution is a virtue in Crawl, as it is in
many other roguelikes, there are strong forces driving characters deeper.

Interface
========================================

The interface is radically designed to make gameplay easy - this sounds trivial,
but we mean it. All tedious, but necessary, chores should be automated. Examples
are long-distance travel, exploration and taking notes. Also, we try to cater
for different preferences: both ASCII and tiles are supported; as are vi-keys
and numpad. Documentation is plenty, context-specific and always available
in-game. Finally, we ease getting started via tutorials.

Clarity
========================================

Things ought to work in an intuitive way. Crawl definitely is winnable without
spoiler access. Concerning important but hidden details (i.e. facts subject to
spoilers) our policy is this: the joy of discovering something spoily is nice,
once. (And disappears before it can start if you feel you need to read spoilers
- a legitimate feeling.) The joy of dealing with ever-changing, unexpected and
challenging strategic and tactical situations that arise out of transparent
rules, on the other hand, is nice again and again. That said, we believe that
qualitative feedback is often better than precise numbers.

In concrete terms, we either spell out a gameplay mechanic explicitly (either in
the manual, or by in-game feedback) or leave it to min-maxers if we feel that
the naive approach is good enough.

Consistency
========================================

While there is no plot to speak of, the game should still be set in a consistent
Crawl universe. For example, names of artefacts should fit the mood, vaults
should be sensibly placed and monsters should somehow fit as well. Essentially,
this is about player immersion. As such, it's good to have in mind, but
consistency is always secondary to gameplay. A typical example is player vs.
monster behaviour: while we try to make these identical (or similar), there are
good reasons for keeping them distinct in certain cases.

Replayability
========================================

This is actually quite important, but in some sense just a corollary to the
major design goals. Besides these, there are several other points helping to
make playing Crawl fun over and over again:

Diversity
  whenever there are choices to the player, be that choice of species, god,
  weapon or spell, the various options should be genuinely different. It is no
  good to provide dozens of weapons with different names (and perhaps even
  numbers) if, in the end, they all play the same.

Many different species
  This is partly due to the skills and aptitude system. Similarly important are
  the built-in starting bonuses/handicaps of species; these often have great
  impact on play. To us, balance does not mean that all combinations of
  background and species play equally well! Some are much more challenging than
  others, and this is fine with us. Each species has at least some backgrounds
  playing rather well, though.

Dungeon layout
  Even veteran players may find the Tomb or the Hells exciting (which are
  designed such that life endangering situations can always pop up). These and
  other branches may or may not fit a given character's buildup. By the way, we
  strongly believe that games are pointless if you can reach the invincible
  state.

Religion
  This addresses new players, as getting to the Temple and choosing a god
  becomes the first major task of most games. But religion is also a point in
  favour of replayability for experienced players, since the choice of god can
  matter as much as species does.

Playing styles
  Related to, but encompassing, species, background, god are fundamentally
  different playing styles like melee oriented fighter, stabber, etc. Deciding
  on whether (and when!) to make a transition of style can make or break games.

Out of the depths
========================================

From time to time a discussion about Crawl's unfair OOD (out of depth) monsters
turns up, like a dragon on the second dungeon level. These are not bugs!
Actually, they are part of the randomness design goal. In this case, they also
serve as additional motivation: in many situations, the OOD monster can be
survived somehow, and the mental bond with the character will then surely grow.
OOD monsters also help to keep players on their toes by making shallow levels
still not trivial. In a similar vein, early trips to the Abyss are not deficits:
there's more than one way out, and successfully escaping is exciting for anyone.

########################################
Appendices
########################################

****************************************
1. List of character species
****************************************

Species are categorised, roughly, by how difficult and complex they are to
learn how to play, into three categories: *Simple*, *Intermediate*, and
*Advanced*. These categories do not necessarily align with difficulty for an
experienced Crawl player, but rather are intended as an indication of to what
degree a species has unusual or complex mechanics, or requires deeper/wider
knowledge of how the game works. (For example, Djinn have one of the higher
win rates of all species, but are classified as "Intermediate" because
their no-mp/no-books mechanic takes some adapting to, and has non-trivial
interactions with background and god choice.) Despite being the outcome of a
discussion among many players, these categorizations definitely have a
subjective element to them, and you shouldn't take them to be limiting!

The order within categories is also, roughly, determined by our best judgment
about the relative ease of learning to play each species.

Next to each species name, in parentheses, is the canonical abbreviation for
the species.

.. note:: Use 'A' to check for which particular peculiarities a species might
          have. Also, some species have special abilities which can be accessed
          by the 'a' abilities menu. Some also have physical characteristics
          which allow them to make extra attacks.

.. note:: Humans are a useful reference point when considering other species:
          they have 0 for almost all aptitudes; have no special abilities,
          weakness, or constraints against using certain types of equipment;
          move normally; and gain experience and willpower at a "typical"
          rate. However, you will see that they are categorised as an
          *Intermediate* species -- because they are decent, but not excellent,
          at nearly everything, a Human may need to make use of all sorts of
          game mechanics depending on what they find in the dungeon, and know
          how to defend itself against any type of damage or attack it
          encounters.

Simple species
==============

Species categorised as *Simple* work straightforwardly for players who have
less experience with Crawl's game mechanics. While many do have quirks, these
quirks tend to be passive traits that simplify gameplay, rather than challenges
that a player has to consciously work around. While all of these species do
have weaknesses of some kind, these weaknesses are simple to understand, aren't
fundamentally crippling to all members of the species, and are balanced by
other strengths. In many cases the special properties of these species allow
the player to set aside many aspects of the game while still developing a
strong character.


Mountain Dwarves (MD)

  Mountain Dwarves are stout and hardy folk, adept at fighting with axes and
  blugeoning weapons, though lacking the dexterity to excel at other forms of
  combat. Their reserves of magic are somewhat poor, though they still make
  passable spellcasters, and their connection with the blood of the earth gives
  them a particular talent at fire and earth magics. Their spell success is
  significantly less encumbered by armour than other species.

  They are superlative artisans and smiths, employed in ancient times by even the
  gods themselves, and this spiritual history makes them exceptional at invoking
  divine aid. They can even use enchantment scrolls to improve artefacts that
  would be beyond the understanding of any other species.

Minotaurs (Mi)
  The Minotaurs are a species of hybrids, possessing Human bodies with bovine
  heads. They delve into the Dungeon because of their instinctive love of
  twisting passageways.

  Minotaurs are extremely good at all forms of physical combat, but are awful at
  using any type of magic. They can wear all armour except for some headgear.
  When in close combat, Minotaurs are able to reflexively headbutt those who
  dare attack them.

Merfolk (Mf)
  The Merfolk are a hybrid species of half-Human, half-fish that typically live
  in the oceans and rivers, seldom venturing toward land. However, Merfolk
  aren't as limited on land as some myths suggest; their tails will quickly
  reform into legs once they leave the water (and, likewise, their legs will
  quickly reform into a tail should they ever enter water). They tend to be
  surprisingly nimble on land as well as in the water. Experts at swimming,
  they need not fear drowning and move very quickly through water.

  The Merfolk have developed their martial arts strongly on thrusting and
  grappling, since those are the most efficient ways to fight underwater. They
  therefore prefer polearms and short swords above all other weapons, though
  they can also use longer swords quite well.

  As spellcasters, they tend to be quite good in specific areas. Their mystical
  relationship with water makes it easier for them to use alchemy and ice magic,
  which use water occasionally as a material component. The legendary water
  magic of the Merfolk was lost in ancient times, but some of that affinity
  still remains. Most other magic seems foreign to them.

Gargoyles (Gr)
  A cross between ordinary stone gargoyles and living beings, Gargoyles are
  hideous humanoids with an affinity to rock. They have low health, but large
  amounts of innate armour which increases further as they gain levels. They
  eventually gain the ability to fly.

  Gargoyles' partially living form grants them immunity to poison, as well as
  resistance to electricity, and protection from some effects of necromancy.
  Their natural armour makes them strong melee fighters, and they are naturally
  skilled with blunt weapons and in unarmed combat. They can also be exceptional
  earth-based conjurers.

Draconians (Dr)
  Draconians are Human-dragon hybrids: humanoid in form and approximately
  Human-sized, with wings, tails and scaly skins. Draconians start out in an
  immature form with brown scales, but as they grow in power they take on a
  variety of colours. This happens at an early stage in their career, and the
  colour is determined by chromosomes, not by behaviour.

  Most types of Draconians have breath weapons or special resistances.
  Draconians cannot wear body armour and advance very slowly in levels, but are
  reasonably good at all skills other than missile weapons, and they develop
  natural physical defences that compensate for the lack of body armour, without
  needing to train their Armour skill at all. Still, each colour has its own
  strengths and some have complementary weaknesses, which sometimes requires a
  bit of flexibility on the part of the player. They are good general-purpose
  spellcasters, and typically their spellcasting aptitudes will adapt slightly
  when they gain a colour.

  Draconian colours are detailed below, in the subsection titled
  `Draconian types`_.

Trolls (Tr)
  Trolls are monstrous creatures with powerful claws. They have thick, knobbly
  skins of any colour from putrid green to mucky brown, which are covered in
  patches of thick fur.

  They are incredibly strong, and regenerate rapidly from even the most terrible
  wounds. However, they are hopeless at spellcasting and learn most skills very
  slowly. Their large size prevents them from wearing most forms of armour.

Deep Elves (DE)
   The Deep Elves are a species of Elves who long ago fled the overworld to live
   in darkness underground. There, they developed their mental powers, evolving
   a natural gift for all forms of magic, and adapted physically to their new
   environment, becoming weaker and losing all colouration. They are poor at
   melee combat and physical defence, although they are capable at using bows
   and other ranged weapons.

Armataurs (At)
  The Armataurs are a large, scaled mammalian species, walking on four feet
  and swinging a powerful tail behind them. Their elephant-back armies
  terrorise the lands outside the Dungeon.

  Armataurs instinctively roll when moving toward foes, getting a free move and
  regenerating magic. They have great aptitudes with armour and shields, though
  their body shape reduces the protection offered by body armour early on. At
  higher levels they also regenerate both health and magic when rolling, making
  them truly resilient.

Gnolls (Gn)
  Gnolls are a species of caniform humanoids originally hailing from the arid
  deserts and grasslands of the east. In recent history they have become
  unusually attracted to the Dungeon, establishing tribes around and even
  inside of it. Unfortunately their long stay in the Dungeon has exposed their
  somewhat fragile minds to excessive amounts of its magic.

  On the one hand, their bizarrely altered brains now have incredible
  proficiency at learning every skill. On the other, these same alterations
  have rendered Gnolls incapable of selective learning. They learn all skills
  at the same time, so are generally unable to specialise in any one thing.

  In order to survive with this limitation, Gnolls use their universal
  knowledge to take advantage of every resource they find in the Dungeon. They
  also have powerful noses adapted to the Dungeon's scents, allowing them to
  easily locate where treasures lay hidden.


Intermediate Species
====================

Species classified as *Intermediate* require a broader understanding of the
mechanics of Crawl, have some weakness(es) that must be actively compensated
for, and/or add a relatively complex mechanic (or change in mechanic) to
gameplay.

Humans (Hu)
  Humans are natural explorers. As they uncover new spaces in the dungeon,
  they are refreshed and invigorated, rapidly healing and recovering magic.
  They are also the most versatile of all species - having balanced aptitudes
  for all skills lets them adapt to use whatever they find.

Kobolds (Ko)
  Kobolds are small, mysterious creatures of unknown origin. They are well
  suited to lurking in the darkness of the Dungeon, and have a reduced range of
  vision which also reduces the range at which they can be seen by enemies.

  They are competent in combat, especially with short blades, maces or ranged
  weapons, and are comfortable with all forms of magic. They are also very
  adept at using magical devices. Their small size makes them unable to wield
  large weapons, but they are agile and stealthy, and advance in levels slightly
  more quickly than Humans.

Demonspawn (Ds)
  Demonspawn are horrible half-mortal, half-infernal creatures. Demonspawn can
  be created in any number of ways: magical experiments, breeding, unholy pacts,
  etc. Although many Demonspawn may initially be indistinguishable from those of
  pure mortal stock, they will inevitably grow horns, scales or other unusual
  features. Powerful members of this class of beings also develop a range of
  unholy abilities, which are listed as mutations.

  Demonspawn advance slowly in experience and learn most skills slightly slower
  than Humans, although they are talented at some forms of magic. They learn
  Invocations especially quickly, although the good gods will not accept their
  worship due to their unholy nature.

Djinn (Dj)
  Djinn are beings of smokeless fire. They enter the world spontaneously and
  without explanation, born with a tireless hunger for knowledge and adventure.
  Djinn have a unique relationship with magic: rather than learning spells
  from books, their spells come from within, welling up from their fiery core
  as they gain experience. They draw from that same fiery core to cast spells -
  for Djinn, magical power and health are one and the same.

  As elemental beings, Djinn are immune to poison and highly resistant to
  fire, though cold damage is deeply inimical to them. Since they float
  through the air without need for legs or feet, they cannot wear boots.

  Djinn are middlingly competent at most forms of physical combat, but have
  a particular aptitude for spellcasting. Their aptitudes for all forms of
  magic are phenomenal, though their unique relationship with magic means that
  they cannot choose to train magic skills independently.

Spriggans (Sp)
  Spriggans are small magical creatures distantly related to Elves. They love to
  frolic and cast mischievous spells.

  They are poor fighters and have little physical resilience, but they move
  extremely quickly and stealthily, and are incredible at dodging attacks. They
  are terrible at destructive magic - conjurations, summonings, necromancy and
  elemental spells. On the other hand, they are excellent at other forms of
  magic and at evoking magical items. Their size makes them unable to wear most
  armour. They cannot wield large weapons, and even most smaller weapons require
  both hands to be wielded by a Spriggan.

Ghouls (Gh)
  Ghouls are horrible undead creatures that sleep in their graves for years on
  end, only to rise and stalk the living. Slain foes heal these monstrous
  beings as they feast on the macabre energies released.

  They learn most skills slowly, although they make decent unarmed fighters
  with their claws. Due to their contact with the grave they can also learn to
  use ice, earth, and necromantic magic without too many difficulties.

  Like other undead, ghouls are naturally immune to poisons, negative energy
  and torment; have little warmth left to be affected by cold; and are not
  susceptible to mutations.

Tengu (Te)
  The Tengu are an ancient and feared species of bird-people with a legendary
  propensity for violence. Basically humanoid with bird-like heads and clawed
  feet, the Tengu can wear all types of armour except helmets and boots. Their
  magical nature helps them evade attacks while in motion, and despite their
  lack of wings, more experienced Tengu can magically fly.

  They are experts at all forms of fighting, including the magical arts of
  combat (conjurations, summonings and, to a lesser extent, necromancy). They
  are good at air and fire elemental magic, but poor at ice and earth magic.
  Tengu do not appreciate any form of servitude, and so are poor at using
  invocations. Their light avian bodies cannot sustain a great deal of injury.

Oni (On)
  Oni are large, rowdy creatures who love a good fight. They are exceptionally
  strong and robust, and their fondness for drink allows them to heal twice as
  much from healing potions and even perform free melee swings around themselves
  while they chug them.

  They are proficient with most melee weapons and forms of magic, but lack the
  dexterity or inclination to use ranged weapons or magical devices well. They
  are, however, good at throwing things, in particular large rocks.

  Their large size prevents them from wearing most forms of armour, and are poor
  at dodging, relying on their enormous bulk to survive battles instead.

Barachim (Ba)
  Barachim are an amphibious humanoid species, spawned at the dawn of time as
  servants for the gods. Inevitably, they rebelled and fled into the mortal
  world; but even uncounted years later, the darkness still flees at their
  approach, remembering those who they once served.

  Barachim's most remarkable trait is their grossly overmuscled legs, which
  allow them to leap great distances. When not leaping, they are somewhat
  slow-moving, and the long sight-lines that their heritage creates can be a
  major disadvantage, but they can master almost any skill.

Advanced Species
================

*Advanced* species have some substantial weaknesses, and/or add multiple complex
new mechanics to gameplay. This category includes several species that
experienced players may not find difficult per se, but that may require quite
a bit of experience to adapt to. It also includes species that are just
plain difficult, such as Mummies.

Coglins (Co)
   Unlike most of their goblin kin, Coglins augment their tiny frames with
   charm-wrought steel. Their exoskeletons, capable of wielding weapons
   independently in each arm, are begun with hand-me-downs from their sprawling
   families. By the time they come of age, they are inseparable from their
   creations, and experienced individuals often further customise themselves
   with uniquely powerful additions.

   Haphazard and jerry-rigged, Coglin exoskeletons lack the flexibility of
   natural bodies. Though their arms can fly in a whir of destruction, it
   takes quite a bit of swinging to rev them up to that speed. Further, with
   their bodies swallowed in the controls of their machine, they cannot wear
   jewelry - the interference of multiple magical fields would be catastrophic!

   Even so, a true Coglin would never criticise their creation. They believe
   that spirits of steel and sandalwood come to rest within every thing that
   draws blood, slowly and carefully re-attuning their exoskeletons to those
   spirits whenever they wield or remove weapons. Never scorn the spirits!

Vine Stalkers (VS)
  Limber in shape, Vine Stalkers are anthropomorphic masses of thick vines.
  They possess a once-humanoid core, parasitised moments before death by the
  magical vines. Lacking any other discernible features, their faces are
  dominated by the disproportionate, vicious maw with which they disrupt and
  devour the magical energies of their foes.

  Magic courses freely through their bodies, and any damage they take is split
  between their health and magical reserves. They also physically regenerate
  at an alarming rate. However, these traits come at a price: the dual nature of
  their bodies makes them extremely frail, and they cannot benefit from potions
  to heal their wounds.

  Living examples of adaptation, Vine Stalkers level up quickly and lend well
  to an all-out offensive style; trusting their stealth to choose their prey
  and then their regenerating capabilities to power through the wounds they may
  sustain in battle. Many members of the species, however, are seen wielding
  magic quite competently and then switching to a hybrid style when their
  reserves start to run low, thus replenishing their shroud of magic and their
  spells' fuel with each voracious bite.

Vampires (Vp)
  Vampires are an undead species, with the ability to shift between bloodless
  and alive forms at will. Bloodless Vampires can heal themselves by drinking
  the blood of the living in combat, and have the traits of the undead (immunity
  to poisons, negative energy and torment, and resistance to damage from cold),
  but are much less resilient and cannot regenerate health when monsters are in
  sight. On the other hand, a Vampire full with blood will regenerate very
  quickly, but will lose all undead powers. Upon growing, they learn to
  transform from their bloodless form into a fast-moving bat. Unlike other
  undead species, they may be mutated normally at all times.

Demigods (Dg)
  Demigods are mortals with some divine or angelic ancestry, however distant.
  Demigods look more or less like members of their mortal part's species, but
  they are extremely robust and can draw on great supplies of magical energy.
  They are able to sculpt their attributes to a far greater extent than any
  other species, gaining substantial boosts to their choice of Strength,
  Intelligence or Dexterity as they gain experience. On the downside, they
  advance more slowly in experience than any other species, gain skills slightly
  less quickly than Humans and, due to their status, refuse to worship any god.

Formicids (Fo)
  The Formicids are a species of humanoid ants. Just like their tiny insect
  ancestors, the Formicids are well adept at earth work, both on the physical
  and magical sides. Their abilities have been used to tunnel immense
  underground communities and structures, many of which are tens of thousands of
  years old.

  Perhaps unfortunately, their strong ties to earth have left them completely
  impervious to being teleported or hasted; Formicids are tied to the earth with
  a complete sense of stasis. While this is a seemingly bad property for a
  dungeon adventurer, stasis has the beneficial effect of preventing many types
  of nasty hexes and maledictions.

  With the ability to lift ten times their own weight, the Formicids have
  strength rivaling that of Oni. This, along with the fact that they have four
  arms, allows Formicid warriors to equip both a shield and a two-handed weapon
  at the same time.

  Formicids make good earth mages and alchemists, but are quite capable at both
  melee and ranged combat too. They are naturally bad at air magic and
  conjurations.

Nagas (Na)
  Nagas are a hybrid species: Human from the waist up with a large snake tail
  instead of legs.

  They are reasonably good at most things and advance in experience levels at
  the same rate as Humans. They are naturally resistant to poisons, can see
  invisible creatures, and have tough skin, but their tails are relatively slow
  and cannot move them around as quickly as can other creatures' legs (this only
  affects their movement rate; all other actions are at normal speed). Like
  Armataurs, their body shape also prevents them from gaining full protection
  from body armour. A Naga's biggest forte is stealth: Nagas are very good at
  moving unnoticed. Their tails eventually grow strong enough to constrict
  their foes in combat.

  Nagas can spit poison; the accuracy and damage of this poison increases with
  the Naga's experience level.

Octopodes (Op)
  These land-capable relatives of common octopuses can move about as fast as
  Humans and yet retain the ability to swim underwater, although their dual
  adaptation is not as good as that of the shapechanging Merfolk.

  Octopodes have eight tentacle-shaped legs, and need four of them to move.
  While a tentacle lacks fingers, two tentacles are a rough equivalent of a
  Human's arm where item manipulation is concerned - including wielding
  two-handed weapons with four. They can use no armour other than loose hats,
  but can handle shields just fine. Another peculiarity they have is the ability
  to wear eight rings, one on each tentacle.

  Their natural camouflage makes them excel at stealth, and they have good
  knowledge of alchemy as well. They are also able to use their tentacles to
  constrict enemies - potentially several at a time!

Felids (Fe)
  Felids are a breed of cats that have been granted sentience. Originally they
  were witches' familiars that were magically augmented to provide help for
  their masters' rituals, yet many have abandoned, outlived, or, in at least one
  case, eviscerated their former masters and gone out into the world.

  While fully capable of using speech and most forms of magic, Felids are at a
  serious disadvantage due to their inability to use armour or weapons.

  Their agility and stealth are legendary, as is their ability to get to hard to
  reach places. Felids advance in levels very slowly. They are skilled with many
  forms of magic, though less so with raw elemental magic.

  Felids start with an extra life, and gain more as they increase in levels.
  Upon death, they will be resurrected in a safe place.

Mummies (Mu)
  These are undead creatures who travel into the depths in search of revenge,
  redemption, or just because they want to.

  Mummies progress slowly in levels, half again as slowly as Humans in all
  skills except fighting, spellcasting and necromancy. The sacred embalming
  rituals that brought them into unlife also grant them a special connection
  with the divine, and as they increase in levels, they become increasingly
  in touch with the powers of death. However, their desiccated bodies are
  highly flammable. They also cannot drink.

  Like other undead, mummies are naturally immune to poisons, negative energy
  and torment; have little warmth left to be affected by cold; and are not
  susceptible to mutations.

Draconian types
========================================

Red Draconians
  feel at home in fiery surroundings. They are bad with ice magic but very
  proficient with fire. They can breathe highly combustible embers which
  cause a fiery explosion whenever they hits a foe.

White Draconians
  stem from frost-bitten lands, and are naturally resistant to frost. They
  beathe piercing cold which encases anything it kills in a solid block of
  ice. They are versed in ice magic, but bad at fire.

Green Draconians
  are used to venomous surroundings and breathe clouds of mephitic vapours. They
  are especially good in the arts of alchemy and without deficiencies in other
  magic realms. Later on, they will develop a poisonous stinger.

Yellow Draconians
  have a sulphurous breath full of corrosive acid, and are naturally acid
  resistant. Their knowledge of corrosion makes them especially good in the
  maintenance of Forgecraft machinery. Later on, they gain an acidic bite
  attack.

Grey Draconians
  can breathe torrents of mud which impede the movement and attacks of
  non-flying enemies. They are proficient with earth magic but bad with air
  magic. Later on, iron fuses onto their scales to make them hardier than
  other Draconians.

Black Draconians
  can unleash arcing electrical discharges, and are naturally insulated. They
  are good at air magic but feel cumbersome with earth magic. Their wings will
  eventually grow larger, which allows them to fly when combined with their
  natural skill with air magic.

Purple Draconians
  are highly adapted to all spellcasting in general, and to hexes in
  particular. They are a bit better at evoking things than most other
  Draconians. They can breathe dispelling energy which strips those it hits
  of enchantments and impairs their spellcasting. They are stronger-willed
  than other draconians, and later on, they gain resistance to both external
  mana draining and to enchantment stripping.

Pale Draconians
  are better at air and fire magic, and have no deficiencies in other schools.
  They breathe sight-obscuring steam and, like their Purple cousins, have a
  slight advantage at Evocations.

****************************************
2. List of character backgrounds
****************************************

In your quest, you play as one of a large number of different types of
characters. Although each has its own strengths and weaknesses, some are
definitely easier than others, at least to begin with. The best backgrounds for
a beginner are probably Gladiators and Berserkers; if you really want to play a
magician, try a Conjurer or a Hedge Wizard. However, not all species are equally
well suited for all backgrounds. After you have selected a species, the background
selection menu will show backgrounds generally considered to be more accessible
for a species in a brighter colour.

Each background starts out with a different set of skills and items, but from
there you can shape them as you will. Note that due to peculiarities of size or
body shape, some species-background combinations start with a different
inventory than described here.

Warrior backgrounds
===================

Warriors are experienced at using physical weapons and defending themselves.

Fighters
  Fighters usually start with a good weapon of their choice, a suit of medium
  armour, a shield, and a potion of might.

Gladiators
  The Gladiator has been trained to fight in the ring. They start with a good
  weapon of their choice, light armour, headgear and some throwing weapons and
  nets.

Monks
  Monks have a head start with the divine. They start with only a simple weapon
  of their choice, a potion of divine ambrosia, a robe, and an orb of light to
  guide them. However, when they worship a god for the first time, their
  spiritual training gives them a piety boost.

Hunters
  The Hunter is a type of fighter who specialises in missile weapons. A Hunter
  starts with a shortbow, a scroll of butterflies, and a set of leathers.

Brigands
  A Brigand is a shady character who is especially good at killing, using
  daggers or darts. They start with a dagger, a robe and cloak, poisoned darts,
  and a few deadly and rare curare darts.

Zealot backgrounds
==================

Zealots start the game already worshipping a god.

Berserkers
  Berserkers are hardy warriors who worship Trog the Wrathful, from whom they
  get the power to go berserk (as well as a number of other powers, should they
  prove worthy), but who forbids the use of spell magic. They enter the Dungeon
  with a weapon of their choice, and dressed in animal skins.

Chaos Knights
  The Chaos Knight is a plaything of Xom, subject to the god's constantly
  changing moods. Xom is a very unpredictable (and possibly psychotic) entity
  who rewards or punishes according to whim. They begin with a lightly enchanted
  leather armour, a simple weapon of their choice, and a scroll of butterflies.

Cinder Acolytes
  Cinder Acolytes serve Ignis, the Dying Flame, who grants them incredible
  power over fire... but there is only so much fire left to draw on, and once
  it burns out, acolytes may need to abandon Ignis. They start with a robe,
  a choice of flaming weapons, and the spell Scorch.

Adventurer backgrounds
======================

Adventurers have varied and idiosyncratic skills that they have picked up in
their travels.

Artificers
  Artificers have built, bought or burgled an assortment of magic wands to
  help them through the early Dungeon. Wands have a limited number of uses,
  though, so they'll want to upgrade from their club ASAP.

Shapeshifters
  Shapeshifters use talismans to shift their body into different forms,
  granting them uncanny power but making them unable to use some items.
  They enter the dungeon with two talismans and a potion of lignification.

Wanderers
  Wanderers are "jacks-of-all-trades, masters of none". They start the game
  with a random assortment of skills, items, and maybe spells.

Delvers
  Delvers have, through some mishap, found themselves several floors below the
  surface of the Dungeon. They're equipped with a wide variety of magical escape
  tools, and are well advised to use them to travel to earlier dungeon floors as
  quickly as possible.

Warrior-mage backgrounds
========================

Warrior mages begin the game with a mix of physical combat and magic skills,
though usually excel at neither. They start with a library of spells and
(usually) some way of defending themselves.

Warpers
  Warpers specialise in translocation magic, and are experts in travelling long
  distances and positioning themselves precisely and use this to their advantage
  in melee or missile combat. They start with a scroll of blinking, a selection
  of translocation spells, some dispersal darts, a simple weapon of their choice,
  and leather armour.

Hexslinger
  Hexslingers use debilitating spells to assist their ranged attacks. They
  begin the game with a sling, some spells to support its use, a scroll of
  poison to keep foes at a distance, and a robe.

Enchanters
  The Enchanter specialises in the subtle art of hexes. Instead of directly
  damaging foes, hexes disable and debilitate them, allowing the Enchanter to
  finish the helpless creatures in combat. The Enchanter begins with a lightly
  enchanted dagger, a robe, potions of invisibility, and a selection of hexes.

Reaver
  Reavers have an assortment of powerful, but highly situational, conjurations
  to draw upon when their skill in melee is insufficient. They start with a
  simple weapon of their choice and leather armour.

Mage backgrounds
================

A mage is not an available character background by itself, but a type of
background, encompassing Hedge Wizards, Conjurers, Summoners, Necromancers,
Forgewrights, various Elementalists and Alchemists. Mages are the best at using
magic. Among other things, they start with a robe, a potion of magic, and spells
which should see them through the first several levels of the Dungeon.

Hedge Wizards
  A Hedge Wizard is a magician who does not specialise in any area of magic.
  Hedge Wizards start with a variety of magical skills and with Magic Dart
  memorised, from a large library of varied low-level spells. They also get a
  wizard hat.

Conjurers
  The Conjurer specialises in the violent and destructive magic of conjuration
  spells. Like Wizards, the Conjurer starts with the Magic Dart spell, in their
  case from a library of destructive conjurations.

Summoners
  The Summoner specialises in calling creatures from this and other worlds to
  give assistance. Although they can at first summon only very weak creatures,
  the more advanced summoning spells allow summoners to call on such powers as
  hydras and dragons.

Necromancers
  The Necromancer is a magician who specialises in the less pleasant side of
  magic. Necromantic spells are a varied bunch, but many involve some degree of
  risk or harm to the caster.

Forgewright
 The Forgewright specialises in the creation of magical constructs, ranging from
 simple weapons to elaborate and powerful golems. Many of their creations
 benefit from fighting alongside them.

Elementalists
  Elementalists are magicians who specialise in one of the four types of
  elemental magic: air, fire, earth or ice.

  Fire Magic
    tends towards indiscriminate, wide-range destructive conjurations and
    starting fires.

  Ice Magic
    offers diffuse, subtle effects, both defensive and offensive.

  Air Magic
    provides powerful but difficult to direct spells.

  Earth Magic
    offers direct effects, some destructive and some debilitating.

Alchemist
  Alchemists start with knowledge of poison-based magic, which is extremely
  useful in the shallower levels of the Dungeon where few creatures are immune
  to it.

****************************************
3. List of skills
****************************************

Here is a description of the skills you may have. You can check your current
skills with the 'm' command, and therein toggle between progress display and
aptitude display using '*'. You can also read the table of aptitudes from the
help menu using '?%', and during character choice with '%'.

Fighting skills
========================================

Fighting is the basic skill used in ranged and melee combat, and applies no
matter which weapon your character is wielding (if any). Fighting is also the
skill that determines the amount of health your character gains as they
increase in levels (note that this is calculated so that you don't get a long
run advantage by starting out with a high Fighting skill). Unlike the specific
weapon skill, Fighting does not change the speed with which you make your
attacks.

Weapon skills affect your ability to fight with specific melee weapons. Weapon
skills include:

  * Short Blades
  * Long Blades
  * Maces &amp; Flails
  * Axes
  * Staves
  * Polearms

If you are already good using a class of weapons, say Long Blades, you'll get
a bonus to using similar weapons, like Short Blades; this is called
crosstraining and is shown in blue in the skill menu. Similar types of weapons
are:

  * Short Blades and Long Blades
  * Maces &amp; Flails and Axes
  * Polearms and Axes
  * Staves and Polearms
  * Staves and Maces &amp; Flails

Being good at a specific weapon improves the speed with which you attack with it.
Both the base speed and the best (lowest) possible speed are displayed in the
inventory entry for a weapon. Although lighter weapons are easier to use
initially, as they strike quickly and accurately, heavier weapons increase in
damage potential very quickly as you improve your skill with them. You can check
the current delay of your weapon by swinging it at air (using ctrl-direction) and
looking at the number in parentheses next to your turncount.

Some weapon types have special abilities. Axes are able to cleave through
multiple enemies in a single swing, hitting enemies in an arc around the
wielder with every attack. Polearms can reach farther and allow the wielder to
attack an opponent two squares away, and even reach over monsters. Use the 'v'
command to target a specific monster with a reaching attack, or use Autofight
('tab') to reach automatically.

Finally, Unarmed Combat skill increases the accuracy, damage, and speed of
attacks made while unarmed. Note that most auxiliary attacks, such as an
Armataur's tail-slap or a Minotaur's headbutt, are not affected by Unarmed
Combat. The only exception is the off-hand punch attack granted by using
neither weapon nor shield, which Unarmed Combat makes somewhat more effective.

Ranged combat skills
========================================

Ranged Weapons is the skill for bows, crossbows, and slings, whereas Throwing
governs all things hurled without a launcher: boomerangs, javelins, nets,
darts, etc.

Just as with melee weapons, ranged weapon skills and throwing skills increase
the speed at which you attack, along with slightly increasing your accuracy
and damage. Missile weapons, unlike melee or throwing weapons, are slowed by
wearing heavy armour. Increasing your Strength and Armour skill will partially
mitigate this.

Magic skills
========================================

Spellcasting is the basic skill for magic use. It affects your reserves of
magical energy (Magic) in the same way that Fighting affects your health: every
time you increase the Spellcasting skill you gain some magic points, and you
gain a spell level every time you reach a skill level divisible by 0.5.
Spellcasting also helps with the power and success rate of your spells, but to
a lesser extent than the more specialised magical skills.

There are also individual skills for each different type of magic; the higher
the skill, the more powerful the spell. Multidisciplinary spells use an average
of the two or three skills.

Miscellaneous skills
========================================

Armour
  Having a high Armour skill means that you are skilled at wearing armour of all
  kinds, multiplying the protection provided not just by body armour but also
  by cloaks, gloves, etc. It also very slightly mitigates the penalties to
  spellcasting and missile weapon speed from wearing heavy armour.

Dodging
  A high Dodging skill helps you to evade melee and ranged attacks more
  effectively. This is more easily done in light armour, but can still be useful
  in heavier armour.

Stealth
  Helps you avoid being noticed, and makes monsters more likely to lose track of
  you when you leave their line of sight. Wearing heavy armour penalises stealth
  attempts. Large creatures (like Trolls) are bad at stealth, except for Nagas,
  which are unusually stealthy.

  Stealth also helps you make a very powerful first strike against a
  sleeping/resting monster who hasn't noticed you yet. This is most effective
  with a dagger, slightly less effective with other short blades and Felid claws,
  and less useful (although still by no means negligible) with any other weapon.

  Stealth also improves some melee attacks against confused, distracted, or
  otherwise incapacitated monsters, though this is much less effective than when
  the monster is asleep or paralysed.

  Note that in addition to the bonus from weapon type, there is an additional
  stabbing bonus based on the average of your stealth skill and your skill with
  your wielded weapon.

Shields
  Affects the amount of protection you gain by using a shield, and the degree to
  which it hinders your evasion, attack speed and spellcasting success.
  Mastering the Shields skill removes all penalties from using a shield.

Invocations
  Affects your ability to call on your god for aid. Those skilled at Invocations
  have reduced failure rates and produce more powerful effects. Some gods (such
  as Trog) do not require followers to learn this skill, or use a different
  skill for their abilities instead (such as Necromancy under Kikubaaqudgha).

  Invocations can increase your maximum magical reserves, although it has a
  smaller effect than Spellcasting in this regard. The bonuses are not cumulative:
  the highest contribution from Spellcasting or Invocations is used.

Evocations
  This skill lets you use wands much more effectively, in terms of both damage
  and precision. Similarly, various other items that have evocable powers work
  better for characters trained in this skill.

****************************************
4. List of keys and commands
****************************************

Main screen
========================================

Crawl has many commands to be issued by single key strokes. This can become
confusing, since there are also several modes; here is the full list. Some
commands are particularly useful in combination with certain interface options;
such options are mentioned in the list. For a description of them, please look
into options_guide.txt. For a more terse list of all commands, use '??' in-game.
Most modes (targeting, level map, interlevel travel) also have help menus via
'?' on their own.

Movement
----------------------------------------

direction
  This moves one square. The direction is either one of the numpad cursor keys
  (try both NumLock on and off) or one of the Rogue vi keys (hjklyubn).

Shift-direction or / direction
  This moves straight until something interesting is found (like a monster). If
  the first square is a trap, movement starts nonetheless.

o
  Auto-explore. Setting the option explore_greedy to true makes auto-explore run
  to interesting items (those that get picked up automatically) or piles
  (checking the contents). Autoexploration will open doors on its own unless
  you set travel_open_doors to avoid or approach.

G or Ctrl-G
  Interlevel travel (to arbitrary dungeon levels or waypoints). Remembers old
  destinations if interrupted. This command has its own set of shortcuts; use ?
  for help on them.

Ctrl-W
  Set waypoint (a digit between 0 and 9). Go to a waypoint by pressing Ctrl-G
  or uppercase G, then the waypoint's digit.

Attacking and firing
----------------------------------------

Several of these commands enter targeting mode; see `Targeting`_ for more
information about this mode.

direction
  If a monster is in the target square, attack that monster.

Tab
  Autofight: Attack the nearest monster with your current weapon. If the
  nearest monster is not in range, by default, this will move towards it.

v
  Targeted attacks with your primary weapon, including attacking non-adjacent
  monsters with a polearm, or firing a wielded launcher (regardless of the
  state of the main quiver).

Q
  Quiver an item, spell, or ability from a menu.

( and )
  Cycle quiver to next/previous suitable action (item, spell, ability).

f
  Fire currently quivered action, showing a targeter. If some monster is in
  sight and the action takes a target, either the last target or the nearest
  monster will be automatically highlighted. If the action does not take a
  target, the display will typically show an area of effect. Pressing f again
  triggers the action.

Shift-tab, p
  Autofire: Fire a quivered action, if needed selecting a target automatically;
  typically fires at the nearest monster.

F
  Directly choose ammo to throw or fire. In contrast to 'f' this does not
  interact with the quiver.

V
  Evoke an item directly from the inventory. This includes using wands.

Spells and abilities
----------------------------------------

Spells and abilities may also be quivered; see `Attacking and firing`_. Many
spells and abilities enter targeting mode on activation; see `Targeting`_ for
more information about this mode.

a
  Show the ability menu, allowing you to activate an ability or read its
  description.

z
  Cast a spell. Should the spell demand monsters as targets but there are none
  within range, casting will be stopped. In this case, neither turns nor magic
  are used. If you want to cast the spell nonetheless, use Z.

Z
  Cast a spell regardless of available targets.

Resting
----------------------------------------

s, Del, . or Numpad 5
  Rests for one turn. This is most often used tactically for waiting a few
  turns. Serious resting should be done with the 5 command, for the sake of
  your keyboard and sanity.

5 or Shift-Numpad 5
  Long resting, until both health and magic points are full.

Resting is generally indistinguishable from any other action; healing, magic
point restoration, etc, proceed at the same rate, whether you're resting or not.
A few specific spells can be 'channeled' via the rest key for ongoing effects,
as mentioned in their descriptions.

Dungeon interaction
----------------------------------------

O
  Open door. This is also done automatically by walking into the door.

C
  Close door.

Ctrl-direction or * direction
  Opens/closes a door in the specified direction (if there is one), or
  else attacks without moving (even if no monster is seen).

<
  Use staircase to go higher, or use a shop, altar, or portal.

>
  Use staircase to go deeper, or use a shop, altar, or portal.

;
  Examine occupied tile and auto-pickup eligible items. Can also be used to pick
  up only part of a stack with no other item on the same square.
  When a monster is present the first press of ; will only examine the tile
  and a second press of ; will pick up all auto-pickup eligible items.

x
  Examine surroundings, see below. Has '?' help.

X
  Examine level map, see below. Has '?' help.

Ctrl-X
  Lists all monsters, items and features in sight. You may read their
  descriptions and travel to an item or feature.

Ctrl-O
  Show dungeon overview (branches, shops, etc.).

!
  Annotate a level. You can annotate any level of a branch of which you have
  found the entrance. You can enter any text. This annotation is then listed in
  the dungeon overview (Ctrl-O) and also shown whenever you enter that level
  again. Should your annotation contain an exclamation mark (!), you will be
  prompted before entering the level. An empty string clears annotations.

Character information
--------------------------------------

'display' below means usage of the message area, 'show' means usage of the whole
screen.

@
  Display character status.

[
  Display worn armour.

}
  Display list of runes collected.

"
  Display worn jewellery.

E
  Display experience info.

^
  Show religion screen.

A
  Show abilities/mutations.

\\
  Show item knowledge. You can toggle autopickup exceptions for item types in
  this screen. The screen has its own help text.

m
  Show skill screen. You can get descriptions of present skills from that
  screen, as well as the aptitudes. The screen has its own help text.

i
  Show inventory list. Inside this list, pressing a slot key shows information
  on that item.

I
  Show list of memorised spells.

%
  Show resistances and general character overview: health, experience, money,
  gear, and status, mutations, abilities (the latter three more terse than with
  the command @, A, a). This is a highly condensed conglomeration of [, ", E, ^,
  @, A, a, $ on a single screen. Pressing the key of a displayed item views it.

Other game-playing commands
----------------------------------------

t
  Tell commands to allies, or shout (with tt).

Ctrl-A
  Toggle autopickup. Note that encounters with invisible monsters always turns
  autopickup off. You need to switch it on with Ctrl-A afterwards.

\|
  Toggle various display layers and overlays. (Console only)

\`
  Re-do previous command

0
  Repeat next command a given number of times

Non-game playing commands
----------------------------------------

?
  The help menu.

Ctrl-P
  Show previous messages.

Ctrl-R
  Redraw screen.

Ctrl-C
  Clear main and level maps.

#
  Dump character to file (name.txt).

:
  Add note to dump file (see option take_notes).

?:
  Read the notes in-game.

?V
  Display version information.

?/
  Describe a monster, spell or feature. You can enter a partial name or a regex
  instead of the full name.

~ or Ctrl-D
  Add or save macros and key mappings.

=
  Reassign inventory/spell/abilities letters.

_ (console) or F12 (WebTiles)
  Read messages (when playing online; not for local games).

\-
  Edit player doll (Tiles only).

Saving games
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

S
  Save game with query and exit.

Ctrl-S
  Save game without query and exit.

Ctrl-Q
  Quit without saving (with a confirmation prompt).

Stashes
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Ctrl-F
  Find. This searches in stashes and shops, you can use regular expressions and
  also terms like 'long blades', 'shop', 'altar', 'artefact'. If you are looking
  for altars to a special god, a search for 'Trog' , etc. works. A string like
  'D:13' will list all known items on that level. 'D:1}' will search for items
  on level 1 only, as opposed to 'D:1', which will also list items on D:10
  through D:15. Once the list of all found places is displayed, you can cause
  auto-travel to go there (press the associated letter) or you can examine the
  items (press ? followed by the letter).

Item interaction (inventory)
========================================

See also `Attacking and firing`_ for item interaction commands related to
attacking and firing.

{
  Inscribe item (check the autoinscribe option). An empty inscription or
  inscribing only space will remove prior inscriptions. See Appendix `5.
  Inscriptions`_. You can also inscribe items when viewing them by pressing
  their slot key in the inventory.

q
  Quaff a potion.

r
  Read a scroll.

M
  Memorise a spell from a book.

w
  Wield an item ( - for none).

'
  Wield item a, or switch to b.

W
  Wear armour.

T
  Take off armour.

P
  Put on jewellery.

R
  Remove jewellery.

e
  Equip actions (wield, wear, put on)

c
  Unequip actions (unwield, take off, remove)

Item interaction (floor)
========================================

g or ,
  Pick up items. Use a prefix to pick up smaller quantities. As with dropping,
  Ctrl-F allows you to pick up items matching regular expression.

d
  Drop an item. Within the drop list, you can select slots based on a regular
  expression by pressing Ctrl-F, followed by the regex.

d#
  Drop exact number of items, where # is a number.

D
  Drop item(s) picked up last.

Shortcuts in lists (like multidrop)
========================================

When dropping, the drop menu accepts several shortcuts. The same applies to
the pickup menu. In the following, if an item is already selected, the key
will deselect it (except for ',' and '-', obviously).

(
  Select all missiles.

)
  Select all hand weapons.

[
  Select all armour.

?
  Select all scrolls.

/
  Select all wands.

\|
  Select all staves.

!
  Select all potions.

=
  Select all rings.

"
  Select all amulets.

}
  Select all miscellaneous items.

,
  Global select (subject to drop_filter option).

\-
  Global deselect (subject to drop_filter option).

\*
  Invert selection. This will allow you to select all items even if you use the
  drop_filter option.

.
  Selects next item. (If you have pressed the key of an item in the list, '.'
  will toggle the next item. This can be repeated, quickly selecting several
  subsequent items).

;
  Select last unequipped. Selects the equipment (armour, jewellery, or weapon)
  you last took off or unwielded.

Level map ('X')
========================================

The level map (brought up by 'X' in the main screen) uses the whole screen to
show the dungeon.

Esc, Space
  Leave level map.

?
  Level map help.

\-
  Scroll level map up.

\+
  Scroll level map down.

direction
  Move cursor.

Shift-direction
  Move cursor in bigger steps (determined by the the option
  level_map_cursor_step).

.
  Travel to cursor (also Enter, Del, ',' and ';'). If the cursor is on the
  character, move cursor to last travel destination instead.

o
  Move cursor to the next autoexplore target.

v
  Describe remembered feature or monster under the cursor.

<
  Cycle through up stairs.

>
  Cycle through down stairs.

^
  Cycle through traps.

_
  Cycle through altars.

Tab
  Cycle through shops and portals.

I
  Cycle forward through all items and piles.

O
  Cycle backward through all items and piles.

G
  Select another level (by branch and depth) to view the map of.

[
  View the previous level.

]
  View the next level.

!
  Add an annotation to the current level.

Ctrl-C
  Clear level and main maps (from temporarily seen monsters, clouds, etc.).

Ctrl-F
  Forget level map.

Ctrl-U
  Restore forgotten level map.

Waypoints can be set on the level map. You can travel to waypoints using G.
The commands are:

Ctrl-W
  Set waypoint.

W
  Cycle through waypoints.

Travel exclusions mark certain spots of the map as no-go areas for autotravel
and explore.

e
  Set travel exclusion. If an exclusion is already present, change size (from
  single square to full field of vision); after that, remove exclusion.

R#
  Set an exclusion with an arbitrary radius, where # is a number from 1 to 8.
  If an exclusion is already present, change its radius to #.

Ctrl-E
  Erase all travel exclusions at once.

E
  Cycle through travel exclusions.

Examining surroundings ('x')
========================================

When roaming the dungeon, the surroundings mode is activated by 'x'. It lets
you look at items, monsters or other features in line of sight.

Esc, Space, x
  Return to playing mode.

?
  Special help screen.

\* or '
  Cycle objects forward.

/ or ;
  Cycle objects backward.

\+ or =
  Cycle monsters forward.

\-
  Cycle monsters backward.

direction
  Move cursor.

. or Enter
  Travel to cursor (also Del).

v
  Describe feature or monster under the cursor. Some branch entries have
  special information.

>
  Cycle downstairs.

<
  Cycle upstairs.

_
  Cycle through altars.

Tab
  Cycle shops and portals.

Ctrl-X
  Lists all monsters, items and features in sight. You may read their
  descriptions and move the cursor to an item, monster, or feature.

Targeting
========================================

Targeting mode is similar to examining surroundings. It is activated whenever
you fire projectiles, evoke a wand or cast spells which use targets. All of the
commands described for examination of surroundings work, with the exception of
Space (which fires).

Esc or x
  Stop targeting.

?
  Special help screen.

Enter
  Fire at cursor direction (also Del and Space).

.
  Fire at cursor position and stop there. This can be useful to avoid damaging
  allies, or to avoid losing arrows.

p
  Fire at previous target (if still in sight).

f
  Smart-firing: fire at previous target, if it is still in sight; and else fire
  at the cursor position. You can start shooting at an opponent with 'ff' and
  then keep firing with 'ff'.

:
  Toggle display of the beam path.

( and )
  When 'f'iring, these two commands allow you to cycle between quiverable
  actions (items, spells, abilities).

Shift-direction
  Fire straight in that direction.

Ctrl-X
  Lists all valid targets. You may select a target from the list to move the
  cursor to that target.

****************************************
5. Inscriptions
****************************************

You can use the { command to manually inscribe items; alternatively, you can
also inscribe when viewing items from the inventory (done by pressing the item's
letter). This adds a note in curly braces to the item description. Besides
simply allowing you to make comments about items, there are several further
uses.

Inscriptions as shortcuts
========================================

You can use inscriptions to define shortcuts for easy access to items,
regardless of their actual inventory letter. For example, if an item's
inscription contains "@w9", you can type 'w9' in order to wield it. Instead of
the 9, any other digit works as well. And instead of 'w'ield, any other command
used for handling items can be used: 'r'ead, 'q'uaff, e'v'oke, 'f'ire, etc.
Using "@*9" will make any action command followed by '9' use this item.

Safety inscriptions
========================================

Inscriptions containing the following strings affect the behaviour of some
commands:

!*
  Prompt before any action using this item.

!w
  Prompt before wielding and unwielding this item.

!a
  Prompt before attacking when wielding this item. Non-weapons and ranged
  weapons prompt automatically. Also, if you answer 'y', you won't be prompted
  again until you switch weapons. To reset this prompt while keeping the
  current weapon wielded, use 'w' and select the current weapon.

!d
  Prompt before dropping this item.

!e
  Prompt before equipping this item.

!u
  Prompt before unequipping this item.

!q
  Prompt before quaffing this item.

!r
  Prompt before reading this item.

!f
  Prompt before firing or throwing this item.

!W
  Prompt before wearing this armour.

!T
  Prompt before taking off this armour.

!P
  Prompt before putting on this jewellery.

!R
  Prompt before removing this jewellery.

!v
  Prompt before evoking this item.

=g
  Pick this item up automatically if autopickup is on.

=f
  Exclude this item from automatic quivering.

\+f
  Include this item in automatic quivering.

=F
  Exclude this item when cycling quiver actions.

\+F
  Include this item when cycling quiver actions.

!Q
  Prompt before explicitly quivering this item. Entails =F,=f.

=R
  Do not offer to swap out this piece of equipment if another one could be
  removed instead.

!D
  Prompt before performing an action that might destroy this item. It won't
  protect against lava or deep water accidents.

You can use the autoinscribe option to have some items automatically inscribed.
See options_guide.txt for details. Some examples are:

  autoinscribe = stone:=f
  autoinscribe = wand of heal wounds:!v

Artefact autoinscriptions
========================================

Artefacts are automatically inscribed with abbreviated descriptions of their
properties. The inscriptions use the following general ideas:

rXXX
  signifies a resistance; e.g. rF+ (a level of fire resistance), rN+++ (three
  levels of negative energy resistance), rC- (cold vulnerability).

\+XXX
  signifies an ability you can evoke via the 'a' command. E.g. +Inv (evocable,
  temporary invisibility).

\-XXX
  signifies a suppressed ability. E.g. -Cast (spellcasting forbidden).

XX+6
  means a boost to some numerical stat (similar with XX-2, etc.). E.g. Slay+3
  (+3 to accuracy and damage of melee and ranged combat).

For more information, examine an item (by selecting it in your (i)nventory);
each property of an artefact will be listed and described.

****************************************
6. Dungeon sprint modes
****************************************

Dungeon sprints are short, faster-paced variants of Crawl that aim at more of a
"coffee break" style of gameplay. They can be accessed from the main menu, and
save files coexist with regular game saves. Sprint modes all use the basic
Crawl mechanics, but with certain aspects of the game sped up. All sprint
games take place on a single map. The most noticeable speed change is much more
rapid experience gain, as well as piety growth. Many sprint modes focus on some
specific aspect of the game, and several introduce variant mechanics or items
unique to that mode. In all of the sprint maps, you win by finding and escaping
with the Orb of Zot, but the conditions for finding the Orb vary.

Sprint I: "Red Sonja"
  Red Sonja, originally called "dungeon sprint 1", is the original dungeon
  sprint map, putting the quest for the Orb of Zot into a single level.

Sprint II: "The Violet Keep of Menkaure"
  Menkaure has stolen the Orb of Zot and hidden it away in his keep; can you
  find it?

Sprint III: "The Ten Rune Challenge"
  Originally introduced in the 2010 tournament, this sprint map places 10 runes
  and the Orb of Zot in a single dungeon level.

Sprint IV: "Fedhas' Mad Dash"
  Fedhas' Mad Dash is a shorter sprint map, a contest organised by Fedhas Madash.
  The monsters are Fedhas-themed; watch out for the many flavours of Oklobs! As
  usual, your goal is to find the Orb of Zot and then escape.

Sprint V: "Ziggurat Sprint"
  Ziggurat Sprint focuses on the Ziggurat mechanic. The map consists of a
  sequence of open-layout rooms of increasing size and difficulty, each with
  many monsters around some theme; rooms are connected by transporters. The
  starting room provides powerful equipment for getting started, as well as
  altars. If you can make it through all rooms, you will be taken to a room with
  the Orb and some exit stairs.

Sprint VI: "Thunderdome"
  In the Thunderdome you face a large set of monsters, including many uniques,
  in a cross-shaped arena. Monsters appear in rounds, with boss rounds every 5
  rounds (every 3 after round 27), and the final boss at round 27. If you kill
  every monster in a normal or boss round, you will get some gold, arena points,
  and a bit of time to rest. In a lightning round, to win you just need to
  survive. A variety of interesting items are available for purchase in shops
  along the sides -- don't miss the QUAD DAMAGE! To win this sprint, kill the
  final boss, and the Orb will appear.

Sprint VII: "The Pits"
  This map is a "traditional-style" sprint map that embeds the quest for the Orb
  of Zot into a dungeon level with three runes.

Sprint VIII: "Arena of Blood"
  In the Arena of Blood, also known as "meatsprint", you are a vessel of Makhleb,
  wielding Makhleb's immensely destructive Axe of Woe against a large variety of
  meat-based monsters in an open arena. To win this mode, kill the Meatlord,
  pick up the Orb of Zot, and escape.

Sprint IX: "|||||||||||||||||||||||||||||"
  This mode, also known as "linesprint", puts the entire game of Crawl in a
  single winding 1-space corridor, with each region as one straight line. Fight
  your way to the middle of the map to claim the Orb of Zot. Escape with it,
  or continue onwards to collect all runes first.
