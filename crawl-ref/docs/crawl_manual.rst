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

While Dungeon Crawl is strictly a single player game, you can interact with
others over a server. Connecting to a server opens several interesting options.

You can:

- watch other players and even communicate with them
- view your past games or those of others
- battle ghosts of other players
- compete using a common score list
- take part in the annual tournament
- play the most recent development version

A full list of available servers and information on how to connect to them can
be found at: http://crawl.develz.org/wordpress/howto

The servers carry no guarantees, though they are generally always running.

There is also a lively IRC channel dedicated to Crawl at ##crawl on irc.freenode.net.
You can ask for help and there will always be someone to watch your game and
give hints if you happen to play on a server.

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

Tutorial for Dungeon Crawl
  Start one of several specialised tutorials to learn how to play.

Hints mode for Dungeon Crawl
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

- The amount of hit points you get each level
- Your rate of skill advancement
- Your initial primary attributes (this also depends on background)
- Various special abilities, powers and limitations

Minor:

- Your rate of level advancement
- Occasional bonus points added to some primary attributes
- The amount of magic points you get each level
- Your innate resistance to hostile enchantments
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
combinations should give you a rough impression of the weaknesses and strengths
of the different species.

For some backgrounds, you must pick a starting weapon before starting the game.

When you start a new character (or load an old one) and want to get a rough
impression, you may examine it with the following commands:

A
  shows any peculiarities like unusual speed or eating behaviours

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
  other ways, too). The main screen shows both your current and maximum hit
  points. Usually, you regain hit points slowly over time. Pressing '5' or
  Shift-Num-5 lets you wait for a longer period.

Magic
  A measure of magic or other intrinsic power. This is used primarily for
  spellcasting, but is sometimes also used for the evoking and invoking of many
  other special abilities. They are displayed in the same way as hit points;
  nothing bad happens if these drop to zero, except, of course, that you can't
  cast any spells. Resting restores these, too. It is difficult to have more than
  50 magic points without using external devices.

Next come your defences. For all of them, more is better.

Armour Class
  Abbreviated to "AC". When something injures you, your AC reduces the amount of
  damage you suffer.

Evasion
  Abbreviated to "EV". This helps you avoid being hit by unpleasant things (but
  will not reduce the amount of damage you suffer if you do get hit).

Shield
  Abbreviated to "SH". This number is a measure of how good your shield (if any)
  is at blocking attacks.

Your character's primary attributes are Strength, Intelligence and Dexterity:

Strength
  Abbreviated to "Str". Increases your damage with melee and ranged weapons.
  Affects your ability to use heavy armours and shields effectively.

Intelligence
  Abbreviated to "Int". Affects how well you can cast spells as well as how much
  nutrition spellcasting takes.

Dexterity
  Abbreviated to "Dex". Increases your accuracy with melee and ranged weapons.
  Significantly affects your ability to dodge attacks aimed at you, your general
  effectiveness with shields, your stealth, and your effectiveness when stabbing
  unaware enemies.

Attributes grow permanently from gaining levels, and may increase or decrease
temporarily from mutations or while using certain artefacts or abilities.

If any attribute drops to zero for some reason, you will experience very
unpleasant side-effects, being slowed and suffering some stat-specific
negative effects. These effects will persist for a short while even after the
attribute is restored.

Upon gaining levels 3, 6, 9, etc., you may choose an attribute to raise. Most
species gain additional attributes at some levels, with the frequency and the
attribute to be increased determined by species.

Finally some additional information about your character and your progress through
the dungeon is displayed.

Experience Level
  Abbreviated to "XL". Starting characters have experience level 1; the highest
  possible level is 27. Gaining a level nets additional hit and magic points,
  and will grant spell slots and sometimes primary attributes.

Place
  This shows the branch you are currently in, as well as the level within the
  branch. The starting branch is called Dungeon, so that the place information
  will read "Dungeon:1" for a new character.

Gold
  Displays the number of gold pieces you have found. Gold is found scattered
  around the dungeon, and is primarily used to buy items from shops.

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

Magic Resistance
  Affects your ability to resist the effects of enchantments and similar magic
  directed at you. Has no effect on direct damage dealt by magic, just on more
  subtle effects. Although your magic resistance increases with your level to
  an extent determined by your character's species, the creatures you will meet
  deeper in the dungeon are better at casting spells, and are more likely to be
  able to affect you. You can get a rough idea of your current MR by pressing
  '@' or '%'.

Stealth
  High stealth allows your character to move through the dungeon undetected.
  It is affected by your species, dexterity, Stealth skill, and the encumbrance
  of your body armour. Your current Stealth level can also been seen by pressing
  '@' or '%'.

There are many ailments or enchantments that can temporarily befall you. These
are noted in the stats area below the experience line. Many of them are
self-explanatory, like Pois or Hungry. Many others, however, can be subtle, and
their effects can be examined by pressing '?/T' and searching for the name of
status effect.

Monsters within your field of vision are listed on a special panel, the monster
list. Single monsters also get indicators of their health status in the form of
a coloured box, and also on effects they enjoy or suffer from. Within target
mode you can directly target single monsters by use of the monster list. Use
'Ctrl-L' to toggle this.

Sometimes characters will be able to use special abilities, e.g. the Naga's
ability to spit poison or the magical power to fly granted by a ring. These are
accessed through the 'a' command.

****************************************
D. Exploring the dungeon
****************************************

Movement
========================================

You can make your character walk around with the numeric keypad (try both
Numlock on and off) or the "Rogue" keys (hjklyubn). If this is too slow, you can
make your character walk repeatedly by pressing Shift and a direction;
alternatively, press '/' followed by a direction. You will walk in that
direction until any of a number of things happen: a hostile monster is visible
on the screen, a message is sent to the message window for any reason, you press
a key, or you are about to step on anything other than normal floor and it is
not your first move of the long walk. Note that this is functionally equivalent
to just pressing the direction key several times.

Another convenient method for moving long distances is described in the section
on Automated Travel and Exploration below.

Resting
========================================

If you press '5', you will rest until your hit points or magic return to full.
You can rest for just one turn by pressing '.' or 's'.

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
get a map of the whole level (which shows where you've already been) by typing 'X'.

You can see the full set of commands available while looking around by pressing
'?', both in the 'x' and 'X' modes.

Staircases and Portals
========================================

You can make your way between levels by using staircases, which appear as ">"
(down) and "<" (up), by pressing the '>' or '<' keys. It is important to know
that most monsters adjacent to you will follow when you change levels; this
holds both for hostile and allied monsters. Notable exceptions are zombies (and
other mindless undead, who are too stupid to properly operate stairs) and ghosts
(who feel they belong to their level).

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
(this is the only way to attack a friendly creature hand-to-hand). If there is
apparently nothing there, you will still attack it, just in case there's
something invisible lurking around.

A variety of dangerous and irritating traps are hidden around the dungeon. Traps
sometimes look like normal floor until discovered.

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
optimisation by default. By manual exploration you can save turns, but auto-explore
will usually save real time.

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

When you kill monsters, you gain experience points (XP). You receive less XP
when friendly creatures took part in killing the monster. When you get enough
XP, you gain an experience level, making your character more powerful. As they
gain levels, characters gain more hit points, magic points, and spell levels.

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
skills already trained (which depends essentially on their background), those
which are not present have to be learned from scratch. Each skill can go up to 27.

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
progress in this area. When you are carrying it, experience used to practise the
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

There are several other less dangerous methods you can use to kill monsters.
Hunters and similarly oriented characters will prefer ranged combat to mindless
bashing. When wielding some launcher, the 'f' and 'F' commands will fire
appropriate missiles. See the section on Targeting in the Items Chapter for
more on this. Likewise, many magicians will prefer to use spells from a safe
distance. They can use the 'z' command to cast spells previously memorised.
Again, see the Targeting section.

Some monsters can be friendly; friendly monsters will follow you around and
fight on your behalf (but they gain some of the normal experience points for any
kills they make, so you get less). You can command your allies using the 't'
key, which lets you shout to attract them or tell them who to attack, or else
tell them to stay where they are or to follow you again. You can also shout to
get the attention of all monsters in range if, for some reason, you want to do
that.

Some special monsters are Uniques. You can identify a unique because he or she
will have a name and personality. Many of these come up with very nasty ideas
how to rid the dungeon of you. Treat them very carefully, particularly if you
meet one of them for the first time.

Other, even rarer, obstacles are statues. A variety of statues can appear,
ranging from harmless granite ones (who still often signify something of
interest) to really dreadful ones. Be alert whenever seeing such a statue.

When playing Crawl, you will undoubtedly want to develop a feeling for the
different monster types. For example, some monsters leave edible corpses and
others do not. Likewise, ranged or magic attackers will prove a different
kind of threat from melee fighters. Learn from past deaths and remember which
monsters pose the most problems. If particular monsters are giving you
trouble, try to alter your tactics for future encounters.

You can obtain information about a monster by using the 'x' (examine) command,
moving the cursor over the monster in question, and pressing 'v' to view the
monster's details; or by searching for a monster by name or symbol with '?/m'.
The details screen shows:

- The monster's name and description.

- Bars indicating its:

  * AC: armour class; how well it ignores most damage

  * EV: evasion; how well it avoids being hit

  * MR: magic resistance; how well it resists most Hexes and similar
    enchantments.

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
(        missiles       (use 'f'ire)
[        armour         (use 'W'ear and 'T'ake off)
%        food           (use 'e'at; also 'c'hop up corpses)
?        scrolls        (use 'r'ead)
!        potions        (use 'q'uaff)
/        wands          (use 'V' to evoke)
=        rings          (use 'P'ut on and 'R'emove)
"        amulets        (use 'P'ut on and 'R'emove)
\\ or |  staves, rods   (use 'w'ield for staves; 'v' for evoking rods)
\+ or :  spellbooks     (use 'r'ead and 'M'emorise and 'z'ap)
}        miscellaneous  (use 'V' for evoking from the inventory)
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

Some items can be sticky-cursed, in which case they weld themselves to your body
when you use them. Such items usually carry some kind of disadvantage: a weapon
or armour may be damaged or negatively enchanted, while rings can have all
manner of unpleasant effects on you. If you are lucky, you might find magic
which can rid you of these curses.

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

Weapons can be enchanted; when you first wield or otherwise identify them,
you reveal values which tell you how much more effective they are than an
unenchanted version. Weapons which are not enchanted are simply '+0'. Some
weapons also have special magical effects which make them very effective
against vulnerable enemies.

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

If you would rather pick off monsters from a safe distance, you will need
missiles for your sling, bow or other appropriate launcher. Several kinds of
missiles, such as javelins, are effective when simply thrown; other kinds
require you to wield an appropriate device to inflict worthwhile damage. Upon
impact, missiles may become destroyed. The chance for this to occur depends on
the type of missile.

The 'f' command fires or throws a missile. The default missile to be fired or
thrown (your "quiver") is displayed on the main screen beneath your wielded
weapon. The quivered item will always be what Crawl thinks is most likely to be
what you want. Thus it will either be an item you previously chose and fired
(with 'f') or directly quivered (with 'Q'), or the item in your inventory that
ranks highest in fire_order - if there are several of similar order, the one
with the lowest inventory slot is chosen.

See Appendix `5. Inscriptions`_ for inscriptions which let you fine-tune the
list of items to choose from. See also the Missiles section of
options_guide.txt.

The firing interface also allows you to manually select an item to throw with
'i'; but it may not be very effective if you lack the correct launcher.

Use the '(', ')' to cycle through your quiver without firing, and 'Q' to choose
the quivered item from a list. If you would like to choose something to fire
without inserting it into the quiver use 'F' instead.

The interface for shooting or throwing things is also used for evoking wands and
casting certain spells, and is described in detail in section I (Targeting).

[ Armour
========================================

This is also rather important. Most worn armour improves your Armour Class,
which decreases the amount of damage you take from most types of injury. The
heavier an armour is, the more AC (armour class) it will provide, at the expense
of your EV (evasion) and stealth. Heavier types of armour also hamper your melee
accuracy, making it harder for you to hit monsters. Wearing heavy armour also
increases your chances of miscasting spells, an effect which is only slightly
reduced by your Armour skill. These penalties are smaller if you have a high
Armour skill, but larger if you have low Strength. On the other hand, body
armour will also provide some guaranteed damage reduction against melee
attacks, and heavier armours are better at this.

A shield normally increases neither your AC nor your evasion, but it lets you
attempt to block melee attacks and some ranged attacks aimed at you. Wearing a
shield (especially larger shields) with insufficient Shields skill makes you
less effective in hand combat and hampers your ability to cast spells. It also
lowers your evasion if you do not have sufficient skill, and you obviously
cannot wield a two-handed weapon while wearing a shield. Shields are most
effective on the first attack on you each turn and become less useful on
every one after that. There are three types: bucklers, shields, and large
shields.

Some magical armours have special powers. These powers are sometimes automatic,
affecting you whenever you wear the armour, and sometimes must be activated with
the 'a' command.

You can wear armour with the 'W' command, and take it off with the 'T' command.
With '[' you can have a quick look at your current gear.

Most armours can be improved by reading the appropriate scroll. Body armour and
bardings can be enchanted up to the base value of AC they provide. Shields can
be enchanted up to +3, +5, or +8, depending on their size. Other gear is limited
to +2.

% Food and Carrion
========================================

Food is extremely important. You can find many different kinds of food in the
dungeon. If you don't eat when you get hungry, you will eventually die of
starvation. Fighting, casting spells, and using some magical items will make you
hungry. When you are starving, you fight much less effectively and cannot cast
spells or use many abilities. You can eat food with the 'e' command.

You may wish to dine on the corpses of your casualties. Despite the fact that
corpses are represented by the same '%' sign as food, you can't eat them without
first cutting off the more edible pieces with the 'c' command. Being hungry helps
you choke down the raw flesh. Chopping up corpses will take some time and will
produce a number of 'chunks', which can be eaten with the 'e' command as above.

Some species are happy to eat raw meat at any time, and others cannot eat meat at
all. Information on special diets is displayed on the 'A' screen.

Vampires are a special case. Members of this species can try to drink blood
directly from a fresh corpse (use the 'e' command). They can also bottle potions
of blood from corpses instead of chopping corpses into chunks with the 'c'
command.

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
potion, which restores some hit points and cures many ailments, but there are
many other varieties of potions to be found. Potions can be quaffed (drunk) with
the 'q' command.

/ Wands
========================================

Sometimes you will be lucky enough to find a stick which contains stored magical
energies. Wands each have a certain amount of charges, and a wand will cease to
function when its charges run out. You must identify a wand to find out how many
uses it has left. This can be done with a scroll of identify; characters with a
good Evocations skill may also deduce the number of charges simply upon evoking
the wand. Evoking a wand without having fully identified the number of charges
remaining will waste some charges.

Wands are aimed in the same way as missile weapons, and you can release the power
of a wand by evoking it with 'V'. See section I for targeting. There are also a
number of wands that may be useful to aim at yourself.

=" Rings and Amulets
========================================

Magical rings are among the most useful of the items you will find in the
dungeon, but can also be some of the most hazardous. Use the 'P' command to
put on rings, and 'R' to remove them. You can wear up to two rings
simultaneously, one on each hand; which hand you put a ring on is immaterial
to its function. If you try to put on a ring while both ring fingers are full,
you will be asked which one to remove. Octopodes are an exception, and may
wear up to eight rings on their tentacles. Some rings function automatically,
while others require activation (with the 'a' command).

Amulets are similar to rings, but have different range of effects. Amulets are
worn around the neck, and you can wear only one at a time.

You can press '"' to quickly check what jewellery you're wearing.

\| Staves
========================================

There are a number of types of magical staves. Some enhance your general
spellcasting ability, while some greatly increase the power of a certain class
of spells (and possibly reduce your effectiveness with others). Some can even be
used in hand-to-hand combat, although with mediocre effectiveness unless you can
harness their special power, using a combination of the Evocations skill and the
skill specific to the staff's type. Staves which do not enhance a destructive
magic school tend to have no combat powers at all.

\| Rods
========================================

Rods ('|') hold unique spells that you can evoke while wielding the rod,
using the 'v' command. The effectiveness of these spells is increased by
Evocations skill. They have a pool of magical energy which regenerates
according to the rod's enchantment (which can be increased using scrolls of
recharging) and your Evocations skill.

: Books
========================================

Most books contain magical spells which your character may be able to learn. You
can read a book with the 'r' command, which lets you access a description of
each spell or memorise spells from it with the 'M' command.

Occasionally you will find manuals of some skill. Carrying these will cause your
experience to have twice the effect as usual when used for training that skill.

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

$ Gold
========================================

Gold can be used to buy items should you run across shops. There are also a
few more esoteric uses for gold.

Artefacts
========================================

Weapons, armour, jewellery and spellbooks can be artefacts. These come in two
flavours: randomly created artefacts ('randarts') and predefined ones
('unrandarts'). Randarts will always carry unusual names, such as "golden
double sword" or "shimmering scale mail". Artefacts cannot be modified in any
way, including enchantments.

Apart from that, otherwise mundane items can get one special property. These are
called 'ego items', and examples are: boots of running, a weapon of flaming, a
helmet of see invisible, and so on. Note that, unlike artefacts, such items can
be modified by enchanting scrolls.

All ego items are noted with special adjectives but not all items noted in this
way need have a special property (they often have some positive or negative
enchantment, though):

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
Spellcasting determines the number of Magic Points available; it also helps in
several respects during the actual casting of spells. Next, there are several
general magical skill areas (Conjuration, Hexes, Charms, Summoning, Necromancy,
Translocation and Transmutation) as well as several elemental areas (Fire, Ice,
Air and Earth) and, finally, Poison. A particular spell can belong to (and thus
allow training of) up to three areas. Being good in the areas of a spell will
improve the casting chance and, in many cases, the effect as well.

Spells are stored in books, which you will occasionally find in the dungeon. You
can read books with 'r' to check what spells they contain; doing so will allow
you to read the individual spells' descriptions. In order to memorise a certain
spell, use the 'M' command.

In addition to picking up new spells, your character may also wish to get rid of
old ones by reading a scroll of amnesia, which will let you pick a spell to
forget.

Each spell has a level. A spell's level denotes the amount of skill required to
use it and indicates how powerful it may be. You can only memorise a certain
number of levels of spells; type 'M' to find out how many. When you gain
experience levels or advance the Spellcasting skill, your maximum increases; you
will need to save up for several levels to memorise the more powerful spells.
When casting a spell, you temporarily expend some of your magical energy and
become hungrier (although high intelligence and Spellcasting help against hunger
from spells). Pressing 'II' (or 'I!') displays the relative hunger costs of your
spells. The hunger cost is approximately proportional to the square of the
number of # marks in this display.

You activate a memorised spell by pressing 'z' (for Zap). Use 'I' to display a
list of all memorised spells without actually casting one. The spells available
are labelled with letters; you are free to change this labelling with the '='
command. You can assign both lowercase and uppercase letters to spells. Some
spells, for example most damage dealing ones, require a target. See the next
section for details on how to target.

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
based on potential severity (with yellow representing a moderate chance, and red
representing a severe chance).

Be careful of magic-using enemies! Some of them can use magic just as well as
you, if not better, and often use it intelligently.

****************************************
I. Targeting
****************************************

When throwing something, evoking certain wands, or casting spells, you are asked
for a direction. There are several ways to tell Crawl which monster to target.

You can press '?' when asked for a direction; this will bring up a help screen.
Otherwise, you use the following commands:

- The cursor will target on the monster which is closest to your position.
  Should you have been firing at something previously, with the offender still
  being in sight, the cursor will instead rest on the previous target.
- Pressing '+' or '=' moves the cursor to the next monster, going from nearer to
  further away. Similarly, '-' cycles backwards.
- Any direction key moves the cursor by one square. Occasionally, it can be
  useful to target non-inhabited squares.
- When you are content with your choice of target, press one key of Enter, Del,
  or Space to fire at the target. If you press '.', you also fire, but the
  spell/missile will stop at the target's square if it misses. This can be
  useful to keep friendlies out of the fire, or to make sure your precious
  missiles won't end up in deep water.
- You can press Escape if you changed your mind - no turns are deducted.

There are some shortcuts while targeting:

- Typing Shift-direction on your keypad fires straight away in that direction.
- Pressing 'p' or 'f' fires at the previous target (if it is still alive and in
  sight). Due to this, most hunters can go a long way by pressing 'ff' to fire
  their ammunition at a monster and then keep firing at it with further 'ff'
  strokes. At times, it will be useful to switch targets with the '+' or '-'
  commands, though.

It is possible to target yourself: obviously beneficial effects like hasting or
healing will actually target the cursor on you, leaving to you only the pressing
of '.', Enter, etc. - except if you want to heal or haste someone else. If you
target yourself while firing something harmful (which can be sensible at times),
you will be asked for confirmation.

Finally, the ':' key allows you to hide the path of your spell/wand/missile.

****************************************
J. Religion
****************************************

There are a number of gods, demons and other assorted powers who will accept
your character's worship, and sometimes give out favours in exchange. You can
use the '^' command to check the requirements of whoever it is that you worship,
and if you find religion to be an inconvenience you can always renounce your
faith (use the 'a' command - but most gods resent being scorned). Further details
can be seen with '!' while in the '^' screen.

To use any powers which your god deems you fit for, access the abilities menu
via the 'a' command; god-given abilities are listed as invocations. The god
Fedhas Madash also has a corpse decay ability specially accessed through 'p'.

Depending on background, some characters start out religious; others have to
pray at an altar to dedicate themselves to a life of servitude. There are altars
scattered all over the dungeon, and there are rumours of a special temple
somewhere near the surface.

At an altar, you can enter a god's service by pressing < or >. You'll first be
given a description of the god, and then be asked if you really want to join.
To see a list of the standard gods and which of their altars you've seen in your
current game, press 'Ctrl-O'. You can also learn about all gods by pressing '?/G'.

Note that some gods are picky about who can enter their service; for example,
good gods will not accept demonic or undead devotees.

If you would like to start the game with a religion, choose your background
from Berserker, Chaos Knight or Abyssal Knight.

****************************************
K. Mutations
****************************************

Although it would doubtless be a nice thing if you could remain genetically
pure, there are too many toxic wastes and mutagenic radiations in the Dungeon
for that to be possible. If your character is so affected by these that he or
she undergoes physiological change, you can use the 'A' command to see how much
of a freak they've become and the 'a' command to activate any mutations which
can be controlled. Many mutations are actually beneficial to your character, but
there are plenty of nasty ones as well. Many mutations have multiple levels,
each of which counts as a single mutation.

You can also become mutated by overusing certain powerful enchantments,
particularly 'Haste' and 'Invisibility', as your system absorbs too much magical
energy. A single use of those effects will never cause dangerous levels of magical
contamination, but multiple uses in short succession, or usage with existing
contamination from other sources (e.g. from miscasting spells) can cause trouble.
Mutations from magical contamination are almost always harmful.

Mutations can also be caused by specific potions, very rare trap effects, or
spells cast by powerful enemies found deep in the dungeon. A few types of monsters
have mutagenic corpses; these will appear in magenta by default.

It is much more difficult to get rid of bad mutations than to get one. A lucky
mutation attempt can actually remove mutations. However, the only sure-fire ways
are to quaff a potion of cure mutation, which will attempt to remove one or more
random mutations, or to join the gods Zin or Jivya, each of whom provides some
remedy against mutations.

Demonspawn are a special case. Characters of this species get certain special
mutations as they gain levels; these are listed in cyan. They are permanent and
can never be removed. If one of your Demonspawn powers has been augmented by a
random mutation, it is displayed in a lighter colour.

Many a species starts with some special intrinsic feats, like the greater speed
of Centaurs or Spriggans, or the eating habits of Trolls, Kobolds and others.
These are often, but not always, like a preset mutation. In case such an innate
feature gets amplified by an ordinary mutation, it is displayed in a light blue
colour.

Some mutations are only temporary and will dissipate after slaying more enemies.
These are listed in purple on the list of mutations, and marked as temporary.

****************************************
L. Licence, contact, history
****************************************

Licence
  See licence.txt for information about Crawl's licensing. Most of the game's
  components are licensed under version 2 or later of the GNU General Public
  License; those that aren't are under compatible licenses.

Disclaimer
  This software is provided as is, with absolutely no warranty express or
  implied. Use of it is at the sole risk of the user. No liability is accepted
  for any damage to the user or to any of the user's possessions.

If you'd like to discuss Crawl, a good place to do so is the official forum:

  https://crawl.develz.org/tavern

All topics related to this game usually meet a warm response, including tales of
victories (going under 'YAVP', i.e. 'Yet Another Victory Post'), especially
first victories (YAFVP) as well as sad stories of deceased characters (being
'YAAD' or 'YASD', i.e. 'Yet Another Annoying/Stupid Death').

Many players, especially those on the online servers, also frequent ##crawl on
the freenode IRC network. This IRC channel also contains many bots providing
helpful information or statistics about the game.

Stone Soup's homepage is at:

  http://crawl.develz.org

Use this page for direct links to downloads of the most recent version. You can
also submit bug reports there at https://crawl.develz.org/mantis. Be sure to
make sure that your bug isn't already in the list. Feature requests should be
posted on the official forum or the development wiki on crawl.develz.org
instead.

The history of Crawl is somewhat convoluted: Crawl was created in 1995 by Linley
Henzell. Linley based Crawl loosely on Angband and NetHack, but avoided several
annoying aspects of these games, and added a lot of original ideas of his own.
Crawl was a hit, and Linley produced Crawl versions up to 3.30 in March 1999.
Further work was then carried out by a group of developers who released 3.40 in
February 2000. Of them, Brent Ross emerged as the single maintainer, producing
versions until 4.0 beta 26 in 2002. After a long period of silent work, he went
a great step by releasing 4.1.2 alpha in August 2005. This alpha contained a lot
of good ideas, but was nearly unplayable due to balance issues. In the meantime,
several patchers appeared, improving Crawl's interface tremendously. Several of
them formed a new devteam; reasoning that rebalancing 4.1.2 was a very difficult
task, they decided to fork Crawl 4.0 beta 26 and selectively include good ideas
from 4.1.2 and other sources. This fork is Stone Soup, and is the game this
manual describes. Stone Soup's release versions were restarted at 0.1 to avoid
confusion with the existing plethora of Crawl versions.

It should be mentioned that there have been other Crawl variants over the years,
among them Ax-Crawl, Tile Crawl and Dungeon Crawl Alternative.

The object of your quest in Crawl (the Orb of Zot) was taken from Wizard's
Castle, a text adventure written in BASIC.

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
for Linux, Windows, and, to some lesser extent, OS X and other Unices). If, for
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
even non-random artefacts cannot be wished for, as scrolls of acquirement
produce random items in general. Likewise, there are no sure-fire means of life
saving (the closest equivalents are controlled blinks, and good religious
standings for some deities).

Anti-grinding
========================================

Another basic design principle is avoidance of grinding (also known as
scumming). These are activities that have low risk, take a lot of time, and
bring some reward. This is bad for a game's design because it encourages players
to bore themselves. Even worse, it may be optimal to do so. We try to avoid
this!

This explains why shops don't buy: otherwise players would hoover the dungeon
for items to sell. Another instance: there's no infinite commodity available:
food, monster and item generation is generally not enough to support infinite
play. Not messing with lighting also falls into this category: there might be a
benefit to mood when players have to carry candles/torches, but we don't see any
gameplay benefit as yet. The deep tactical gameplay Crawl aims for necessitates
permanent dungeon levels. Many a time characters have to choose between
descending or battling. While caution is a virtue in Crawl, as it is in many
other roguelikes, there are strong forces driving characters deeper.

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
OOD monsters also help to keep players on their toes by making shallow, or
cleared, levels still not trivial. In a similar vein, early trips to the Abyss
are not deficits: there's more than one way out, and successfully escaping is
exciting for anyone.

########################################
Appendices
########################################

****************************************
1. List of character species
****************************************

.. note:: Use 'A' to check for which particular peculiarities a species might
          have. Also, some species have special abilities which can be accessed
          by the 'a' abilities menu. Some also have physical characteristics
          which allow them to make extra attacks.

Humans
  Humans tend to be hardworking and industrious, and learn new things quickly.
  The Human species is the most versatile of all the species available to
  players. Humans advance quickly in levels and have equal abilities in most
  skills.

Hill Orcs
  Hill Orcs are Orcs from the upper world who, jealous of the riches which their
  cousins (the Cave Orcs) possess below the ground, descend in search of plunder
  and adventure.

  Hill Orcs are more robust than Humans. Their forte is brute-force fighting,
  and they are skilled at using most hand weapons (particularly axes, with which
  they are experts), though they are not particularly good at using missile
  weapons. Hill Orcs are passable users of most types of magic and are
  particularly skilled with Fire.

  Many Orcs feel superior to all other species and beings, and they have formed
  a religion around that idea. Only Orcs can worship Beogh, the Orc god. They
  can join Beogh even without an altar whenever an orc priest is in sight.

Merfolk
  The Merfolk are a hybrid species of half-human, half-fish that typically live
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
  relationship with water makes it easier for them to use poison and ice magic,
  which use water occasionally as a material component. The legendary water
  magic of the Merfolk was lost in ancient times, but some of that affinity
  still remains. The instability of their own morphogenic matrix has made them
  very accomplished transmuters, but most other magic seems foreign to them.

Halflings
  Halflings, who are named for being about half the size of Humans, live in
  small villages. They live simple lives and have simple interests. Sometimes a
  particularly restless Halfling will leave his or her village in search of
  adventure.

  Halflings are very small but surprisingly hardy for their size, even having an
  innate resistance to mutagenic effects. They can use short blades and shields
  very well, are passable with long blades, and excel in ranged combat with
  slings. They are also very stealthy and dextrous, but are poor at magic
  (except for charms, translocations, and air magic). They advance in levels as
  rapidly as Humans. Halflings cannot wield large weapons.

Kobolds
  Kobolds are small, ugly creatures with few redeeming features. They are not
  the sort of people you would want to spend much time with, unless you happened
  to be a Kobold yourself.

  They tend to be more agile and weaker than Halflings, and are slightly more
  talented at using most types of magic, particularly necromancy. They are
  competent in combat, especially with short blades, maces or crossbows, and are
  also very adept at using magical devices. They often live as scavengers,
  surviving on carrion (which they can eat even when not hungry), but are
  carnivorous and can only eat meat. Kobolds advance in levels as quickly as
  Humans. Like Halflings, Kobolds cannot wield large weapons.

Spriggans
  Spriggans are small magical creatures distantly related to Elves. They love to
  frolic and cast mischievous spells.

  They are poor fighters and have little physical resilience, though they are
  incredibly skilled at dodging attacks. They are terrible at destructive
  magic - conjurations, summonings, necromancy and elemental spells. On the
  other hand, they are excellent at other forms of magic, and are very good at
  moving silently and quickly. So great is their speed that a Spriggan can
  overtake a Centaur. Due to their tiny size, they need very little food.
  However, they are herbivorous and cannot eat meat. Their size also makes them
  unable to wear most armour. They cannot wield large weapons, and even most
  smaller weapons require both hands to be wielded by a Spriggan.

Nagas
  Nagas are a hybrid species: Human from the waist up with a large snake tail
  instead of legs.

  They are reasonably good at most things and advance in experience levels at
  the same rate as Merfolk. They are naturally resistant to poisons, can see
  invisible creatures, and have tough skin, but their tails are relatively slow
  and cannot move them around as quickly as can other creatures' legs (this only
  affects their movement rate; all other actions are at normal speed). Their
  body shape also prevents them from gaining full protection from body armour.
  A Naga's biggest forte is stealth: Nagas are very good at moving unnoticed.
  Their tails eventually grow strong enough to constrict their foes in combat.

  Nagas can spit poison; the range, accuracy and damage of this poison increases
  with the Naga's experience level.

Centaurs
  The Centaurs are another species of hybrid creatures: horses with Human
  torsos. They usually live in forests, surviving by hunting.

  Centaurs can move very quickly on their four legs, and are excellent with bows
  and other missile weapons; they are also reasonable at fighting in general
  while being slow learners at specific weapon skills. They advance quite slowly
  in experience levels and are rather sub-par at using magic. Like Nagas, they
  receive reduced protection from the body armour they wear.

Ogres
  Ogres are huge, chunky creatures who typically are strong rather than smart,
  and not nimble at all. Ogres mature almost as quickly as Humans.

  Their preferred methods of avoiding beatings are dodging and the use of
  shields. Many Ogres find it natural to wield some large and blunt weapon.
  (Countless lethal incidents have taught them to leave most edged weapons be.)
  While all sophisticated forms of missile combat are too awkward for them, they
  are good at throwing things, in particular boulders.

  Contrary to expectations, Ogres are not reduced to mindless brutes. They
  possess a raw talent for witchcraft, letting them pick up the basics of
  spellcasting at an amazing speed. However, the more arcane schools of magic
  are foreign to them and are only learned at poor rates.

Trolls
  Trolls are like Ogres, but even nastier. They have thick, knobbly skins of any
  colour from putrid green to mucky brown, which are covered in patches of thick
  fur, and their mouths are full of ichor-dripping fangs.

  They can rip creatures apart with their claws, and regenerate very quickly
  from even the most terrible wounds. They learn slowly indeed - as slowly as
  High Elves - and need a great amount of food to survive.

Minotaurs
  The Minotaurs are yet another species of hybrids - Human bodies with bovine
  heads. They delve into the Dungeon because of their instinctive love of
  twisting passageways.

  Minotaurs are extremely good at all forms of physical combat, but are awful at
  using any type of magic. They can wear all armour except for some headgear.
  When in close combat, Minotaurs tend to reflexively headbutt those who dare
  attack them.

Tengu
  The Tengu are an ancient and feared species of bird-people with a legendary
  propensity for violence. Basically humanoid with bird-like heads and clawed
  feet, the Tengu can wear all types of armour except helmets and boots. Despite
  their lack of wings, powerful Tengu can fly magically, and very powerful
  members of this species can stay in the air indefinitely. Their movement speed
  and evasion are increased slightly while flying.

  They are experts at all forms of fighting, including the magical arts of
  combat (conjurations, summonings and, to a lesser extent, necromancy). They
  are good at air and fire elemental magic, but poor at ice and earth magic.
  Tengu do not appreciate any form of servitude, and so are poor at using
  invocations. Their light avian bodies cannot sustain a great deal of injury.

Demigods
  Demigods are mortals with some divine or angelic ancestry, however distant;
  they can be created by a number of processes, including magical experiments
  and the time-honoured practice of interplanar miscegenation.

  Demigods look more or less like members of their mortal part's species, but
  have excellent attributes (Str, Int, Dex) and are extremely robust; they can
  also draw on great supplies of magical energy. On the downside, they advance
  more slowly in experience than any other race, gain skills slightly less
  quickly than Humans and, due to their status, cannot worship the various gods
  and powers available to other races.

Demonspawn
  Demonspawn are horrible half-mortal, half-infernal creatures - the flip side
  of the Demigods. Demonspawn can be created in any number of ways: magical
  experiments, breeding, unholy pacts, etc. Although many Demonspawn may
  initially be indistinguishable from those of pure mortal stock, they will
  inevitably grow horns, scales or other unusual features. Powerful members of
  this class of beings also develop a range of unholy abilities, which are
  listed as mutations.

  Demonspawn advance slowly in experience and learn most skills at about the
  same rate as do Demigods. However, they are a little better at fighting
  and conjurations and much better at necromancy and invocations. Note that
  unlike Demigods, they can take on gods, although not all will accept them.

Deep Dwarves
  Deep Dwarves are short, hardy people who, unlike their extinct surface
  relatives, never left the underground homelands. Living there for countless
  generations made them turn pale and lose all ability to regenerate on their
  own, nor are they receptive to any effects which merely hasten regeneration.
  On the other hand, Deep Dwarves have developed the ability to instantly
  counteract small doses of damage. Their empathy with the earth makes them
  sense their surroundings; this ability increases in power as they gain
  experience levels.

  Given their lack of innate healing, few Deep Dwarves venture out for
  adventures or even combat. Those who do bring a wand of heal wounds, or rely
  on divine assistance.

  Naturally, Deep Dwarves are quite adept with all arts of avoiding blows and
  damage. Offensively, they are skilled users of axes, crossbows, and slings.
  Deep Dwarves are highly spiritual beings, often portrayed as actual spirits
  by outsiders; because of this, their skill with invocations is great. They
  are most at home with the magic of earth and death, eventually gaining some
  resistance to the dark powers of necromancy.

  Deep Dwarves can tinker with wands so as to recharge them. However, each time
  they do so, they lose a bit of their magical essence.

Felids
  Felids are a breed of cats that have been granted sentience. Originally they
  were witches' familiars that were magically augmented to provide help for
  their masters' rituals, yet many have abandoned, outlived, or, in at least one
  case, eviscerated their former masters and gone out into the world.

  While fully capable of using speech and most forms of magic, Felids are at a
  serious disadvantage due to their inability to use armour or weapons. Like all
  cats, Felids are incapable of thriving on vegetable food, and need meat to
  survive.

  Their agility and stealth are legendary, as is their ability to get to hard to
  reach places. They move faster than most races, but don't run as fast as
  Centaurs or Spriggans. Felids advance in levels very slowly. They are skilled
  with many forms of magic, though less so with raw elemental magic.

  Felids gain extra lives as they increase in levels. Upon death, they will be
  resurrected in a safe place, losing an experience level in the process.

Octopodes
  These land-capable relatives of common octopuses can move about as fast as
  humans and yet retain the ability to swim underwater, although their dual
  adaptation is not as good as that of the shapechanging merfolk.

  Octopodes have eight tentacle-shaped legs, and need four of them to move.
  While a tentacle lacks fingers, two tentacles are a rough equivalent of a
  human's arm where item manipulation is concerned - including wielding
  two-handed weapons with four. They can use no armour other than loose hats,
  but can handle shields just fine. Another peculiarity they have is the ability
  to wear eight rings, one on each tentacle.

  Their natural camouflage makes them excel at stealth, and they have good
  knowledge of poisons as well. They are also able to use their tentacles to
  constrict enemies - potentially several at a time!

Gargoyles
  A cross between ordinary stone gargoyles and living beings, Gargoyles are
  hideous humanoids with an affinity to rock. They have low health, but large
  amounts of innate armour which increases further as they gain levels. They
  eventually gain the ability to fly continuously.

  Gargoyles' partially living form grants them immunity to poison, as well as
  resistance to electricity, and protection from some effects of necromancy.
  Their natural armour makes them strong melee fighters, and they are naturally
  skilled with blunt weapons and in unarmed combat. They can also be exceptional
  earth-based conjurers.

Formicids
  The Formicids are a species of humanoid ants. Just like their tiny insect
  ancestors, the Formicids are well adept at earth work, both on the physical
  and magical sides. Their abilities have been used to tunnel immense underground
  communities and structures, many of which are tens of thousands of years old.

  Perhaps unfortunately, their strong ties to earth have left them completely
  impervious to being teleported or hasted; Formicids are tied to the earth with
  a complete sense of stasis. While this is a seemingly bad property for a
  dungeon adventurer, stasis has the beneficial effect of preventing many types
  of nasty hexes and maledictions.

  With the ability to lift ten times their own weight, the Formicids have
  strength rivaling that of ogres. This, along with the fact that they have four
  arms, allows Formicid warriors to equip both a shield and a two-handed weapon
  at the same time.

  Formicids make good earth and venom mages, but are quite capable at both melee
  and ranged combat too. They are naturally bad at air magic and conjurations.

Vine Stalkers
  Limber in shape, Vine Stalkers are anthropomorphic masses of thick vines.
  They possess a once-humanoid core, parasitised moments before death by the
  magical vines. Lacking any other discernible features, their faces are
  dominated by the disproportionate, vicious maw with which they disrupt and
  devour the magical energies of their foes.

  Magic courses freely through their bodies, and any damage they take is split
  between their health and magical reserves. They also physically regenerate
  at an alarming rate. However these traits come at a price: the dual nature of
  their bodies makes them extremely frail, and they cannot benefit from potions
  or wands to heal their wounds.

  Living examples of adaptation, Vine Stalkers level up quickly and lend well
  to an all-out offensive style; trusting their stealth to choose their prey
  and then their regenerating capabilities to power through the wounds they may
  sustain in battle. Many members of the species however, are seen wielding
  magic quite competently and then switching to a hybrid style when their
  reserves start to run low, thus replenishing their shroud of magic and their
  spells' fuel with each voracious bite.

Elves
========================================

There are a number of distinct species of Elf. Elves are all physically slight
but long-lived people, quicker-witted than Humans, but sometimes slower to learn
new things. Elves are especially good at using those skills which require a
degree of finesse, such as stealth, sword-fighting and archery, but tend to be
poor at using brute force and inelegant forms of combat. They find heavy armour
to be uncomfortable.

Due to their fey natures, all Elves are good at using magic in general and
elemental magic in particular, while their affinity for other types of magic
varies among the different sub-species.

High Elves
  This is a tall and powerful Elven species who advance in levels slowly,
  requiring half again as much experience as Humans. They have good intelligence
  and dexterity, but suffer in strength. Compared with Humans, they have fewer
  HP but more magic. Among all races, they are best with blades and bows. They
  are not very good with necromancy or with earth or poison magic, but are
  highly skilled with most other forms of magic, especially Air and Charms.

Deep Elves
  This is an Elven species who long ago fled the overworld to live in darkness
  underground. There, they developed their mental powers, evolving a natural
  gift for all forms of magic (including necromancy and earth magic), and
  adapted physically to their new environment, becoming shorter and weaker than
  High Elves and losing all colouration. They are poor at hand-to-hand combat,
  but excellent at fighting from a distance. They advance in levels at the same
  speed as High Elves.

The Undead
========================================

As creatures brought back from beyond the grave, the undead are naturally immune
to poisons, negative energy and torment; have little warmth left to be affected
by cold; and are not susceptible to mutations.

There are three types of undead available to players: Mummies, Ghouls and
Vampires.

Mummies
  These are undead creatures who travel into the depths in search of revenge,
  redemption, or just because they want to.

  Mummies progress slowly in levels, half again as slowly as Humans in all
  skills except fighting, spellcasting and necromancy. As they increase in
  levels, they become increasingly in touch with the powers of death, but cannot
  use some types of necromancy which only affect living creatures. The side
  effects of necromantic magic tend to be relatively harmless to Mummies.
  However, their desiccated bodies are highly flammable. They also do not need
  to eat or drink and, in any case, are incapable of doing so.

Ghouls
  Ghouls are horrible undead creatures, slowly rotting away. Although Ghouls can
  sleep in their graves for years on end, when they rise to walk among the
  living, they must eat flesh to survive. Raw flesh is preferred, and Ghouls
  heal and reverse the effects of their eternal rotting by consuming it.

  They aren't very good at doing most things, although they make decent unarmed
  fighters with their claws and, due to their contact with the grave, can use
  ice, earth and death magic without too many difficulties.

Vampires
  Vampires are another form of undead, but with a peculiarity: by consuming
  fresh blood, they may become alive. A bloodless Vampire has all the traits of
  an undead, but cannot regain lost physical attributes or regenerate from
  wounds over time - in particular, magical items or spells which increase the
  rate of regeneration will not work (though divine ones will). On the other
  hand, a Vampire full with blood will regenerate very quickly, but lose all
  undead powers. Vampires can never starve. They can drink from fresh corpses
  with the 'e' command, or can bottle blood for later use with 'c'. Upon
  growing, they learn to transform into quick bats. Unlike other undead
  species, they may be mutated normally at all times.

Draconians
========================================

Draconians are human-dragon hybrids: humanoid in form and approximately
human-sized, with wings, tails and scaly skins. Draconians start out in an
immature form with brown scales, but as they grow in power they take on a
variety of colours. This happens at an early stage in their career, and the
colour is determined by chromosomes, not by behaviour.

Most types of Draconians have breath weapons or special resistances. Draconians
cannot wear body armour and advance very slowly in levels, but are reasonably
good at all skills other than missile weapons. Still, each colour has its own
strengths and some have complementary weaknesses, which sometimes requires a bit
of flexibility on the part of the player.

Red Draconians
  feel at home in fiery surroundings. They are bad with ice magic but very
  proficient with fire. Their scorchingly hot breath will leave a lingering
  cloud of flame.

White Draconians
  stem from frost-bitten lands, and are naturally resistant to frost. Their
  breath is piercing cold. They are versed in ice magic, but bad at fire.

Green Draconians
  are used to venomous surroundings and breathe clouds of mephitic vapours. They
  are especially good in the arts of poison and without deficiencies in other
  magic realms. Later on, they will develop a poisonous stinger.

Yellow Draconians
  have a sulphurous breath full of corrosive acid, and later gain an acidic bite
  attack. They are acid resistant, too.

Grey Draconians
  have no breath weapon, but also no need to breathe in order to live, which
  helps them survive in deep water. They are proficient with earth magic but bad
  with air magic, and also have harder scales than other Draconians.

Black Draconians
  can unleash huge electrical discharges, and are naturally insulated. They are
  good at air magic but feel cumbersome with earth magic. Their wings will
  eventually grow larger, which allows them to fly continuously when combined
  with their natural skill with air magic.

Mottled Draconians
  are somewhat in touch with fire, yet are not weak with ice. They can spit
  globs of sticky flame at those adjacent to them.

Purple Draconians
  are highly adapted to all spellcasting in general, and to hexes and charms in
  particular. They are a bit better at evoking things than most other
  Draconians. They can breathe dispelling energy which strips those it hits of
  their enchantments, and are naturally more resistant to hostile enchantments
  than other draconians.

Pale Draconians
  are better at air and fire magic, and have no deficiencies in other schools.
  They breathe steam and, like their Purple cousins, have a slight advantage at
  Evocations.

****************************************
2. List of character backgrounds
****************************************

In your quest, you play as one of a large number of different types of
characters. Although each has its own strengths and weaknesses, some are
definitely easier than others, at least to begin with. The best backgrounds for
a beginner are probably Gladiators and Berserkers; if you really want to play a
magician, try a Conjurer or a Wizard. However, not all species are equally well
suited for all backgrounds. The lighter coloured choices on the selection screen
are generally considered to be the more accessible ones.

Each background starts out with a different set of skills and items, but from
there you can shape them as you will. Note that due to peculiarities of size or
body shape, some characters start with a different inventory.

Fighters
  Fighters usually start with a good weapon, a suit of heavy armour, a
  shield, and a potion of might. They have a good general grounding in the
  arts of fighting.

Gladiators
  The Gladiator has been trained to fight in the ring, and so is versed in the
  arts of fighting, but is not so good at anything else. In fact, Gladiators
  have never learned anything except bashing monsters with heavy things. They
  start with a nasty weapon, light armour, headgear and some nets.

Monks
  The Monk is a member of an ascetic order dedicated to the perfection of one's
  body and soul through the discipline of the martial arts. Monks start with
  very little equipment, but can survive without the weighty weapons and
  spellbooks needed by characters of other backgrounds. When they choose a god
  for the first time, their spiritual training gives them a piety boost.

Berserkers
  Berserkers are hardy warriors who worship Trog the Wrathful, from whom they
  get the power to go berserk (as well as a number of other powers, should they
  prove worthy), but who forbids the use of spell magic. They enter the dungeon
  with a weapon of their choice, and dressed in animal skins.

Chaos Knights
  The Chaos Knight is a plaything of Xom. Xom is a very unpredictable (and
  possibly psychotic) entity who rewards or punishes according to whim.

Abyssal Knights
  The Abyssal Knight is a fighter serving Lugonu the Unformed, ruler of the
  Abyss. They are granted some power over the Abyss, and must spread death and
  disorder in return.

Skalds
  Formidable warriors in their own rights, Skalds practice a form of augmenting
  battle magic that is either chanted or sung. Unique to the highlands in which
  they originate, these spells and formulae are second nature: they can either
  inspire greatness in themselves and their allies, or fear in the hearts of
  their enemies.

Warpers
  Warpers specialise in translocation magic, and are experts in traveling long
  distances and positioning themselves precisely and use this to their advantage
  in melee or missile combat. They start with a scroll of blinking.

Assassins
  An Assassin is a stealthy character who is especially good at killing, using
  daggers or blowguns. They start with some deadly curare needles.

Hunters
  The Hunter is a type of fighter who specialises in missile weapons. A Hunter
  starts with either some throwing weapons or a ranged weapon and some
  ammunition, as well as a short sword or club and a set of leathers.

Arcane Marksmen
  Arcane Marksmen are Hunters who use debilitating spells to assist their ranged
  attacks. They are particularly good at keeping their enemies at a distance.

Artificers
  Artificers are attuned to gadgets, mechanics and magic elicited from arcane
  items, as opposed to casting magic themselves. As a consequence, they enter
  the Dungeon with an assortment of wands. Artificers are skilled at evoking
  magical items, and also understand the basics of melee combat.

Wanderers
  Wanderers are people who have not learned a specific trade. Instead, they've
  travelled around becoming "jacks-of-all-trades, masters of none". They start
  the game with a large, random assortment of skills and maybe some small
  items they picked up along the way, but, other than that, they're pretty much
  on their own.

Magicians
========================================

A magician is not an available character background by itself, but a type of
background, encompassing Wizards, Conjurers, Enchanters, Summoners,
Necromancers, Transmuters, various Elementalists and Venom Mages. Magicians are
the best at using magic. Among other things, they start with a robe and a book
of spells which should see them through the first several levels.

Wizards
  A Wizard is a magician who does not specialise in any area of magic. Wizards
  start with a variety of magical skills and with Magic Dart memorised. Their
  book allows them to progress in many different branches of the arcane arts.

Conjurers
  The Conjurer specialises in the violent and destructive magic of conjuration
  spells. Like Wizards, the Conjurer starts with the Magic Dart spell.

Enchanters
  The Enchanter specialises in the subtle art of hexes. Instead of directly
  damaging foes, hexes disable and debilitate them, allowing the Enchanter to
  finish the helpless creatures in combat. The Enchanter begins with lightly
  enchanted weapons and armour, as well as the Corona spell.

Summoners
  The Summoner specialises in calling creatures from this and other worlds to
  give assistance. Although they can at first summon only very wimpy creatures,
  the more advanced summoning spells allow summoners to call on such powers as
  elementals and demons.

Necromancers
  The Necromancer is a magician who specialises in the less pleasant side of
  magic. Necromantic spells are a varied bunch, but many involve some degree of
  risk or harm to the caster.

Transmuters
  Transmuters specialise in transmutation magic, and can cause strange changes
  in themselves and others. They deal damage primarily in unarmed combat, often
  using transformations to enhance their defensive and offensive capabilities.

Venom Mages
  Venom Mages specialise in poison magic, which is extremely useful in the
  shallower levels of the dungeon where few creatures are immune to it.

Elementalists
  Elementalists are magicians who specialise in one of the four types of
  elemental magic: air, fire, earth or ice.

  Fire Magic
    tends towards destructive conjurations.

  Ice Magic
    offers a balance between destructive conjurations and protective charms.

  Air Magic
    provides many useful charms in addition to some unique destructive
    capabilities.

  Earth Magic
    is a mixed bag, with destructive, debilitating and utility spells available.

****************************************
3. List of skills
****************************************

Here is a description of the skills you may have. You can check your current
skills with the 'm' command, and therein toggle between progress display and
aptitude display using '*'. You can also read the table of aptitudes from the
help menu using '?%', and during character choice with '%'.

Fighting skills
========================================

Fighting is the basic skill used in ranged and hand-to-hand combat, and applies no matter
which weapon your character is wielding (if any). Fighting is also the skill
that determines the number of hit points your character gets as they increase in
levels (note that this is calculated so that you don't get a long run advantage
by starting out with a high Fighting skill). Unlike the specific weapon skill,
Fighting does not change the speed with which you make your attacks.

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

Unarmed Combat is a special fighting skill. It allows your character to make a
powerful attack when unarmed and also to punch with the off hand as an
additional melee attack. The Unarmed Combat skill continues to increase your
attack speed while unarmed until it can be raised no more.

Note that auxiliary attacks (such as a Centaur's kick or a Minotaur's headbutt)
are not affected by the Unarmed Combat skill.

Ranged combat skills
========================================

There are a number of individual weapon skills for missile weapons:

  * Throwing (includes blowguns)
  * Bows
  * Crossbows
  * Slings

Throwing is the skill for all things hurled without a launcher: tomahawks,
javelins, tomahawks, nets, etc. The other skills refer to various types of
missiles shot with a launcher. An exception to this are needles: these are
launched using blowguns, an action which uses the Throwing skill. Since
stones can be thrown without launchers to some effect, these skills
crosstrain:

  * Throwing and Slings

Magic skills
========================================

Spellcasting is the basic skill for magic use. It affects your reserves of
magical energy (Magic) in the same way that Fighting affects your hit points:
every time you increase the Spellcasting skill you gain some magic points, and
you gain a spell level every time you reach a skill level divisible by 0.5.
This skill greatly influences the amount by which casting causes hunger.
Spellcasting also helps with the power and success rate of your spells, but to
a lesser extent than the more specialised magical skills.

There are also individual skills for each different type of magic; the higher
the skill, the more powerful the spell. Multidisciplinary spells use an average
of the two or three skills.

Miscellaneous skills
========================================

Armour
  Heavier body armours give more reliable protection from damage but have
  several disadvantages.

  Having a high Armour skill means that you are used to wearing heavy armour,
  allowing you to move more freely and gain more protection. When you look at an
  armour's description (from within the inventory), you can see in particular
  how cumbersome it is. This is measured by the encumbrance rating.

  This skill helps to overcome the evasion penalty of body armours, reduces the
  amount by which heavy armour hampers melee fighting and also somewhat mitigates
  the bad effects of heavy armour on spellcasting. High Armour skill also
  increases the AC provided by other types of armour (gloves, cloaks, etc.).

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
  which it hinders you. For most races, 5/15/25 skill is enough to mitigate the
  encumbrance of bucklers/shields/large shields respectively, though larger
  races need less skill and and smaller races more.

Invocations
  Affects your ability to call on your god for aid. Those skilled at Invocations
  have reduced failure rates and produce more powerful effects. Some gods (such
  as Trog) do not require followers to learn this skill, or use a different
  skill for their abilities instead (such as Necromancy under Kikubaaqudgha).

Evocations
  This skill lets you use wands much more effectively, in terms of both damage
  and precision. Furthermore, with high Evocations, you can easily deduce the
  number of charges in a wand through usage. Similarly, all other items that
  have certain powers (such as crystal balls, decks of cards, or elemental
  summoners) work better for characters trained in this skill.

  Invocations and Evocations can increase your maximum magical reserves,
  although both have a smaller effect than Spellcasting in this regard. The
  bonuses are not cumulative; the highest contribution from Spellcasting,
  Invocations or Evocations is used.

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
  (try both Numlock on and off) or one of the Rogue vi keys (hjklyubn).

Shift-direction or / direction
  This moves straight until something interesting is found (like a monster). If
  the first square is a trap, movement starts nonetheless.

o
  Auto-explore. Setting the option explore_greedy to true makes auto-explore run
  to interesting items (those that get picked up automatically) or piles
  (checking the contents). Autoexploration will open doors on its own except if
  you set travel_open_doors to false.

G or Ctrl-G
  Interlevel travel (to arbitrary dungeon levels or waypoints). Remembers old
  destinations if interrupted. This command has its own set of shortcuts; use ?
  for help on them.

Ctrl-W
  Set waypoint (a digit between 0 and 9). Check the option show_waypoints. You
  can go to a waypoint by pressing Ctrl-G or G and the digit.

Resting
----------------------------------------

s, Del, . or Numpad 5
  Rests for one turn. This is most often used tactically for waiting a few
  turns. Serious resting should be done with the 5 command, for the sake of
  your keyboard and sanity.

5 or Shift-Numpad 5
  Long resting, until both health and magic points are full.

Resting is the only way to get rid of manticore spikes, but is otherwise
indistinguishable from any other action; healing, magic point restoration,
etc, proceed at the same rate, whether you're resting or not.

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
  Annotate current level. You can enter any text. This annotation is then listed
  in the dungeon overview (Ctrl-O) and also shown whenever you enter that level
  again. If you use this command when standing on a staircase, you may also
  annotate the level that staircase leads to. Should your annotation contain an
  exclamation mark (!), you will be prompted before entering the level. An empty
  string clears annotations.

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

a
  Show the ability menu, allowing you to activate an ability or read its
  description.

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

z
  Cast a spell. Should the spell demand monsters as targets but there are none
  within range, casting will be stopped. In this case, neither turns nor magic
  are used. If you want to cast the spell nonetheless, use Z.

Z
  Cast a spell regardless of available targets.

t
  Tell commands to allies, or shout (with tt).

Ctrl-A
  Toggle autopickup. Note that encounters with invisible monsters always turns
  autopickup off. You need to switch it on with Ctrl-A afterwards.

|
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
  through D:19. Once the list of all found places is displayed, you can cause
  auto-travel to go there (press the associated letter) or you can examine the
  items (press ? followed by the letter).

Item interaction (inventory)
========================================

{
  Inscribe item (check the autoinscribe option). An empty inscription or
  inscribing only space will remove prior inscriptions. See Appendix `5.
  Inscriptions`_. You can also inscribe items when viewing them by pressing
  their slot key in the inventory.

f
  Fire quivered missile. If some monster is in sight, either the last target or
  the nearest monster will be automatically targeted. Pressing f again shoots.

F
  Directly choose an item and fire. Contrary to fi this does not change the
  quiver.

( and )
  Cycle quiver to next/previous suitable missile, respectively.

Q
  Quiver item from a menu.

q
  Quaff a potion.

e
  Eat food (tries floor first, inventory next). In the eating prompt, e is
  synonymous to y.

r
  Read a scroll or book.

M
  Memorise a spell from a book.

w
  Wield an item ( - for none).

'
  Wield item a, or switch to b.

v
  Evoke power of wielded item. Also used to attack non-adjacent monsters with
  a polearm.

V
  Evoke an item from the inventory. This includes using wands.

W
  Wear armour.

T
  Take off armour.

P
  Put on jewellery.

R
  Remove jewellery.

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

c
  Chop up a corpse. In case there are several corpses on the ground, you are
  prompted one by one. There, you can answer

  =========  ================================
  y, c       yes (chop up this corpse)
  n, Space:  no (skip this corpse)
  a          yes to all (chop up all corpses)
  q, Esc     stop chopping altogether
  =========  ================================

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

%
  Select all food.

&amp;
  Select all carrion and inedible food.

\+ or :
  Select all books.

/
  Select all wands.

|
  Select all staves.

\\
  Select all rods.

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
  These two commands allow you to change ammunition while targeting. The choice
  is subject to the fire_order option. Usually, you change missiles according
  to your launcher; i.e. when wielding a bow, ( and ) will cycle through all
  stacks of arrows in your inventory.

Shift-direction
  Fire straight in that direction.

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
used for handling items can be used: 'e'at, 'r'ead, 'q'uaff, e'v'oke, 'f'ire,
etc. Using "@*9" will make any action command followed by '9' use this item.

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
  again until you switch weapons.

!d
  Prompt before dropping this item.

!e
  Prompt before eating this item.

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

!Q
  Prompt before explicitly quivering this item.

=g
  Pick this item up automatically if autopickup is on.

=f
  Exclude this item from automatic quivering.

\+f
  Include this item in automatic quivering.

=F
  Exclude this item when cycling ammunition.

\+F
  Include this item when cycling ammunition.

=R
  Do not offer to swap out this ring if another one can be removed instead.

!D
  Prompt before performing an action that might destroy this item. If you're
  attempting to destroy an item thus inscribed by turning it into a snake, or
  burning it in the name of Trog, the game won't even ask you for confirmation
  but will ignore the item. However, it won't protect against lava or deep
  water accidents.

You can use the autoinscribe option to have some items automatically inscribed.
See options_guide.txt for details. Some examples are:

  autoinscribe = royal jelly:=g
  autoinscribe = wand of heal wounds:!v

Artefact autoinscriptions
========================================

Artefacts are automatically inscribed with abbreviated descriptions of their
properties. The inscriptions use the following general ideas:

rXXX
  signifies a resistance; e.g. rF+ (a level of fire resistance), rN+++ (three
  levels of negative energy resistance), rC- (cold vulnerability).

\+XXX
  signifies an ability you can evoke via the 'a' command. E.g. +Fly (evocable,
  temporary flight).

\-XXX
  signifies a suppressed ability. E.g. -Cast (spellcasting forbidden).

XX+6
  means a boost to some numerical stat (similar with XX-2, etc.). E.g. Slay+3
  (+3 to accuracy and damage of melee and ranged combat).

For more information, examine an item (by selecting it in your (i)nventory);
each property of an artefact will be listed and described.
