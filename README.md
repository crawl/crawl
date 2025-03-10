[![Build Status](https://github.com/crawl/crawl/workflows/Build/badge.svg)](https://github.com/crawl/crawl/actions/)

# Dungeon Crawl Stone Soup

Dungeon Crawl Stone Soup is a game of dungeon exploration, combat and magic, involving characters of diverse skills, worshipping deities of great power and caprice. To win, you'll need to be a master of tactics and strategy, and prevail against overwhelming odds.

## Contents

1. [How to Play](#how-to-play)
3. [Community](#community)
5. [How you can help](#how-you-can-help)
4. [License and history information](#license-and-history-information)

## How to Play

For information on how to install the game, please visit [the installation documentation](crawl-ref/INSTALL.md).

If you'd like to dive in immediately, we suggest one of:

* Start a game and pick a tutorial (select tutorial in the game menu),
* Read [quickstart.md](crawl-ref/docs/quickstart.md) (in the [docs/](crawl-ref/docs/) directory), or
* For the studious, [read Crawl's full manual](crawl-ref/docs/crawl_manual.rst).

There is also an ingame list of frequently asked questions which you can access by typing
`?Q`.

#### Internet Play

You can play Crawl online, competing with other players or watching them. Click "Play Online Now!" on [the Crawl homepage](https://crawl.develz.org/) to find your closest server. You can play in your browser or over SSH.

#### Offline Play

Both classical ASCII and tiles (GUI) versions of Crawl are available to [download for Linux, Windows and OS X](https://crawl.develz.org/download.htm).

## Community

* Our official homepage: [crawl.develz.org](https://crawl.develz.org/)
  * Online webtiles play
  * Offline downloads
  * Our community forum: [tavern.dcss.io](https://tavern.dcss.io/)
* [/r/roguelikes Discord](https://discord.gg/S5F2H32)
* The [/r/dcss subreddit](https://www.reddit.com/r/dcss/)
* IRC: #crawl on Libera (or #crawl-dev to chat development)

## How you can help

If you like the game and you want to help make it better, there are a number
of ways to do so. For a detailed guide to the crawl workflow, look at
the [contributor's guide](crawl-ref/docs/develop/contribution-process.md).

### Reporting bugs

At any time, there will be bugs -- finding and reporting them is a great help.
Many of the online servers host the regularly updated development version. Bugs
should be reported to [our github issue
tracker](https://github.com/crawl/crawl/issues). Thoughtful ideas on how to
improve interface or gameplay are welcome, but it's often best to
[discuss](#community) changes before opening an issue or pull request.

### Map making
Crawl creates levels by combining many hand-made (but often randomised) maps,
known as *vaults*. Making them is fun and easy. It's best to start with simple
entry vaults: see [simple.des](crawl-ref/source/dat/des/arrival/simple.des) for
examples. You can also read [the level-design manual](crawl-ref/docs/develop/levels/introduction.txt) for more help.

If you're ambitious, you can create new vaults for anywhere in the game. If
you've made some vaults, you can test them on your own system (no compiling
needed) and submit them via a github pull request. See the [contributor's guide](crawl-ref/docs/develop/contribution-process.md) for details.

### Monster Speech & Item Descriptions
Monster speech provides a lot of flavour. Just like vaults, varied speech depends
upon a large set of entries. Speech syntax is effective but unusual, so you may want to read [the formatting guide](crawl-ref/docs/develop/monster_speech.txt).

Current item descriptions can be read in-game with `?/` or out-of-game
them in [dat/descript/](crawl-ref/source/dat/descript/). The following conventions should be more or less obeyed:
* Descriptions ought to contain flavour text, ideally pointing out major weaknesses/strengths.
* No numbers, please.
* Citations are okay, but try to stay away from the most generic ones.

### Tiles
We're always open to improvements to existing tiles or variants of often-used
tiles (eg floor tiles). If you want to give this a shot, please [contact us](#community). In case you drew some tiles of your own, you can simply share
them with a developer or submit them via a github pull request. See the
[contributor's guide](crawl-ref/docs/develop/contribution-process.md) for
details.

### Patches
For developers (both existing & aspiring!), you can download/fork the source code and write patches. Bug fixes as well as new features are very much welcome.

For large changes, it's always a good idea to [talk with the dev team](#community) first, to see if any plans already exist and if your suggestion is likely to be accepted.

Please be sure to read [docs/develop/coding_conventions.md](crawl-ref/docs/develop/coding_conventions.md) too.

## License and history information

Crawl is licensed as GPLv2+. See [LICENSE](LICENSE) for the full text.

Crawl is a descendant of Linley's Dungeon Crawl. The final alpha of Linley's Dungeon Crawl (v4.1) was released by Brent Ross in 2005. Since 2006, the Dungeon Crawl Stone Soup team has continued development. [CREDITS.txt](crawl-ref/CREDITS.txt) contains a full list of contributors.

Crawl uses the following open source packages; thanks to their developers:

* The Lua scripting language, for in-game functionality and user macros ([license](crawl-ref/docs/license/lualicense.txt)).
* The PCRE library, for regular expressions ([license](crawl-ref/docs/license/pcre_license.txt)).
* The SQLite library, as a database engine ([license](https://www.sqlite.org/copyright.html)).
* The SDL and SDL_image libraries, for tiles display ([license](crawl-ref/docs/license/lgpl.txt)).
* The libpng library, for tiles image loading ([license](crawl-ref/docs/license/libpng-LICENSE.txt)).

Thank you, and have fun crawling!
