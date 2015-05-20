[![Build Status](https://travis-ci.org/crawl/crawl.svg?branch=master)](https://travis-ci.org/crawl/crawl)

NOTE: The most up-to-date source code for Dungeon Crawl Stone Soup is now at https://github.com/crawl/crawl/

Dungeon Crawl Stone Soup
========================

Dungeon Crawl Stone Soup is a game of dungeon exploration, combat and magic,
involving characters of diverse skills, worshipping deities of great power and
caprice. To win, you'll need to be a master of tactics and strategy, and
prevail against overwhelming odds.

Contents:

1. How to get started? (Information for new players)
2. The file system of this version.
3. Contact and reporting bugs.
4. License and history information.
5. How you can help!

There is a list of frequently asked questions which you can access by pressing
?Q in the game.


1. Getting started
------------------

If you'd like to dive in immediately, your best bets are to

* start a game and pick a tutorial (select tutorial in the game menu), or
* read quickstart.txt (in the docs/ directory), or
* for studious readers, browse the manual (see below for all doc files).

#### Internet play:

You can play Crawl online, both competing with other players and
watching them. Check the homepage at
http://crawl.develz.org/wordpress/howto for details, including
information about additional servers. Servers serve either the ASCII based
console version, or the graphical Tiles version that works inside the
browser, or both. See the above link for details. For the console version,
you need a ssh or telnet console; on Windows, the PuTTY program works very
well. Read docs/ssh_guide.txt for a step by step guide on how to set this up.

#### Tiles:

Crawl features an alternative to the classical ASCII display; Tile-based
Crawl is often a lot more accessible to new players. Tiles are available for
Linux, Windows and OS X.

2. File system
--------------

The following files in the Crawl's main folder are essential:

* crawl, crawl.exe

    These start the game. (The actual name depends on your operating system.)

The docs/ folder contains the following helpful texts (all of which can be
read in-game by bringing up the help menu with '?'):

* quickstart.txt

    A very short introduction into the game.

* crawl_manual.txt

    The complete manual; describing all aspects in detail. Contains appendices
    on species, backgrounds, etc.

* options_guide.txt

    Describes all options in detail. The structure of init.txt follows this
    text.

* macros_guide.txt

    A how-to on using macros and keymappings, with examples.

* tiles_help.txt

    An explanation of the Tiles interface.

The settings/ folder contains, among others, the following files:

* init.txt, .crawlrc

    These contain the options for the game. The defaults play well, so don't
    bother with this in the beginning. Permanent death is not an option, but a
    feature!

* macro.txt

    Playing Crawl can be made even more convenient by redefining keys and
    assigning macros. Ignore early on.


3. Contact and reporting bugs
-----------------------------

#### http://crawl.develz.org/
At the official webpage, you can find the Mantis tracker to add bug reports
or upload patches, a wiki to add interface, gameplay and feature suggestions,
as well as sources and binaries. The Mantis tracker is the best way to report
bugs.

#### rec.games.roguelike.misc
This is a Usenet newsgroup dealing with roguelikes, including Crawl. It is
polite to flag your post with -crawl- as other games are discussed over there
as well. This is a good place to ask general questions, both from new players
as well as for spoilers, or to announce spectacular wins.

#### crawl-ref-discuss@lists.sourceforge.net
If you want to chime in with development, you can read the mailing list which
can get pretty busy on the occasion.


4. License and history information
----------------------------------

This is a descendant of Linley's Dungeon Crawl. Development of the main branch
stalled at version 4.0.0b26, with a final alpha of 4.1 being released by Brent
Ross in 2005. Since 2006, the Dungeon Crawl Stone Soup team has been continuing
the development. See the CREDITS in the main folder for a myriad of
contributors, past and present; license.txt contains the legal blurb.

Dungeon Crawl Stone Soup is an open source, freeware roguelike. It is supported
on Linux, Windows, OS X, and, to a lesser extent, on DOS. The source should
compile and run on any reasonably modern Unix.
Stone Soup features both ASCII and graphical (Tiles) display.

Crawl gladly and gratuitously uses the following open source packages; the
text files mentioned can be found in the docs/license/ folder:

* The Lua script language, see lualicense.txt.
* The PCRE library for regular expressions, see pcre_license.txt.
* The SQLite library as database engine; it is properly in the public domain.
* The SDL and SDL_image libraries under the LGPL 2.1 license: lgpl.txt.
* The libpng library, see libpng-LICENSE.txt


5. How you can help
-------------------

If you like the game and you want to help making it better, there are a number
of ways to do so:

#### Playtesting.
At any time, there will be bugs -- playing and reporting these is a great help.
Many of the online servers host the regularly updated development version. Bugs
should be reported to http://crawl.develz.org/mantis/. Besides pointing out bugs,
new ideas on how to improve interface or gameplay are welcome. These can be added
to the development wiki at https://crawl.develz.org/wiki/.

#### Vault making.
Crawl uses many hand-drawn (but often randomised) maps. Making them is fun and
easy. It's best to start with simple entry vaults (look at
dat/des/arrival/simple.des for a first impression). Later, you may read
docs/develop/levels/introduction.txt.
If you're ambitious, new maps for branch ends are possible, as well. If you've
made some maps, you can test them on your system (no compiling needed) and
then add them to http://crawl.develz.org/mantis/.

#### Speech.
Monster talking provides a lot of flavour. Just like vaults, speech depends
upon a large set of entries. Since most of the speech has been outsourced, you
can add new prose. The syntax is effective, but slightly strange, so you may
want to read docs/monster_speech.txt.
Again, changing or adding speech is possible on your local game. If you have
added something, send your additions to http://crawl.develz.org/mantis/.

#### Monster descriptions.
You can look up the current monster descriptions in-game with '?/' or just read
them in dat/descript/monsters.txt. The following conventions should be more or
less obeyed: Descriptions ought to contain flavour text, ideally pointing out
major weaknesses/strengths. No numbers, please. Citations are okay, but try to
stay away from the most generic ones.
If you like, you can similarly modify the descriptions for features, items or
branches.

#### Tiles.
Since version 0.4, tiles are integrated within Crawl. Having variants of
often-used glyphs is always good. If you want to give this a shot, please
contact us via the mailing list. In case you drew some tiles of your own,
you can add them to http://crawl.develz.org/mantis/.

#### Patches.
If you like to, you can download the source code and apply patches. Both
patches for bug fixes as well as implementation of new features are very much
welcome. If you want to code a cool feature that is likely to be accepted but
unlikely to be coded by the devteam, please ask on the IRC channel ##crawl-dev.
Please be sure to read docs/develop/coding_conventions.txt first.

Thank you, and have fun crawling!
