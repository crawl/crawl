# Archeology: major tag version history for dcss
### (https://github.com/crawl/crawl)
### Author: advil

**Overview**. This document tracks changes to `TAG_MAJOR_VERSION`, which is currently found in [tag-version.h](https://github.com/crawl/crawl/blob/master/crawl-ref/source/tag-version.h).  `TAG_MAJOR_VERSION` indicates the major value of a save file versioning system that is independent of dcss version; it has been stable at 34 since 2012 (with many minor version increments, 182 as of Feb 2017). It is generally incremented when the developers at the time decided to drop backwards [save compatibility](https://github.com/crawl/crawl/blob/master/crawl-ref/docs/develop/save_compatibility.txt) with older save files. What this means is that, in principle, any save from any version with the current major tag should be loadable into a newer version of crawl. In some cases save compatibility may go back longer; major version 34 can load saves back to 0.11.0, as of [35673f84](https://github.com/crawl/crawl/commit/35673f843b19) (which is minor version `TAG_MINOR_0_11`). Forwards compatibility is typically blocked by minor version increments, which are not covered in this document.

**History**. This table shows the history of `TAG_MAJOR_VERSION` in master. The dates and version cycles below are from the git history in master, so they may appear out of sync with release dates and other commits (because of commits on branches with later merges, delayed pushes, etc). The `reason` column is not intended to be exact, and in I didn't chase down details. A reason of just "*save compat*" means that the major version was incremented basically because saves from prior versions were so broken due to a *prior* commit that it wasn't fixable (or worth the time to fix). Where the commit includes changes beyond the version increment I have attempted to indicate why the author thought it broke save compatibility.

| Major tag | Time     | Version cycle  | Reason | Commits
|-----------|----------|----------------|--------|-------
| 34        | Aug 2012 | 0.12b          | save compat, monster spells | [ed956313](https://github.com/crawl/crawl/commit/ed9563133047)
| 33        | Apr 2012 | 0.11b          | save compat (merge portal_branches?) | [ca961569](https://github.com/crawl/crawl/commit/ca96156951bf69e87e50d81448e0bce7a0f71a5e), [ccafe4a1](https://github.com/crawl/crawl/commit/ccafe4a1a0dfd12f462219dc943603ec6964cc85)
| 100       | Nov 2011 | (0.11b)          | 1 was a bad choice | [9178e2e1](https://github.com/crawl/crawl/commit/9178e2e14c9cd961589f9f633a513d263759ba2e) (branch)
| 1         | Nov 2011 | (0.11b)          | save compat for branch `portal_branches` | [7b91fc81](https://github.com/crawl/crawl/commit/7b91fc814655) (branch)
| 32        | Jan 2011 | 0.8a           | save compat? | [52b7544e](https://github.com/crawl/crawl/commit/52b7544e8eed3aee8228fbdfe939f11e1bab61aa)
|31         | Nov 2010 | 0.8a           | revert | [d509b89e](https://github.com/crawl/crawl/commit/d509b89e63a49cb4e48bac031cda2d785bd968a0)
| 32        | Nov 2010 | 0.8a           | change in rune behaviour | [3a1f4483](https://github.com/crawl/crawl/commit/3a1f44834a77762edbbf7740f324bdd48e13b52d)
| 31        | Sep 2010 | 0.8a           | save compat | [98cba813](https://github.com/crawl/crawl/commit/98cba81361fe4c7d0564e021d10dc6c99a6b0379)
| 30        | Sep 2010 | 0.8a           | save compat | [a087ed69](https://github.com/crawl/crawl/commit/a087ed692319114f86589f392593ae50d704d48d)
| 29        | Jul 2010 | 0.8a           | map layers | [af76f809](https://github.com/crawl/crawl/commit/af76f809d3f1679210469c3b194f1b06dc85536b)
| 28        | Jul 2010 | 0.8a           | begin 0.8a | [56b6e971](https://github.com/crawl/crawl/commit/56b6e971d9c512fc5c215e8a610988b0f78b956b), [97d9cb97](https://github.com/crawl/crawl/commit/97d9cb9753d3fdd638fddea5eb30041a39c818f3)
| 27        | Jun 2010 | 0.7a           | max monsters change/decoupling | [b4fa663e](https://github.com/crawl/crawl/commit/b4fa663e1c51bf6e459c572eae2ef6a3e8fecaa5)
| 26        | Jun 2010 | 0.7a           | save compat? | [1196655e](https://github.com/crawl/crawl/commit/1196655e8bb65471d04b1f9c13cc130f571e63ae)
| 25        | Jun 2010 | 0.7a           | save compat? | [bb9e8979](https://github.com/crawl/crawl/commit/bb9e897927bc05bdfa700866d3e52b4faef1db36)
| 24        | May 2010 | 0.7a           | save compat (demonic guardian) | [d1767427](https://github.com/crawl/crawl/commit/d1767427b7affeff7902783c3582492a932be4ec)
| 23        | skipped  | N/A            | see note below |
| 22        | Apr 2010 | 0.7a           | .sav filename | [fb0d8da3](https://github.com/crawl/crawl/commit/fb0d8da3be4f01281ed459b550505e78a1053238)
| mid-21    | Apr 2010 | 0.7a           | `tags.h` => `tag-version.h` | [f40f35fa](https://github.com/crawl/crawl/commit/f40f35fa6b8c)
| 21        | Apr 2010 | 0.7a           | save compat (MUT_FEAST) | [01d7a0f5](https://github.com/crawl/crawl/commit/01d7a0f57fef)
| 20        | Apr 2010 | 0.7a           | stats change | [e890227f](https://github.com/crawl/crawl/commit/e890227f44e7)
| 19        | Mar 2010 | 0.7a           | demonspawns | [e9e958e7](https://github.com/crawl/crawl/commit/e9e958e7afd7)
| 18        | Mar 2010 | 0.7a           | melding | [ebad4a41](https://github.com/crawl/crawl/commit/ebad4a412b34)
| 17        | Mar 2010 | 0.7a           | begin 0.7a | [53bd1cca](https://github.com/crawl/crawl/commit/53bd1ccafe47)
| mid-16    | Jan 2010 | 0.6a           | `enum `=> `#define` | [1c8e80d2](https://github.com/crawl/crawl/commit/1c8e80d2d8c2)
| 16        | Jan 2010 | 0.6a           | remove rSlow | [8a78afdc](https://github.com/crawl/crawl/commit/8a78afdcea14)
| 15        | Jan 2010 | 0.6a           | skill enum order change | [7403a3df](https://github.com/crawl/crawl/commit/7403a3dfc98e)
| 14        | Jan 2010 | 0.6a           | brands overhaul | [7e0fff01](https://github.com/crawl/crawl/commit/7e0fff01081a)
| 13        | Dec 2009 | 0.6a           | remove hand crossbows | [bdef58ca](https://github.com/crawl/crawl/commit/bdef58ca99e4)
| 12        | Dec 2009 | 0.6a           | cloud (un)marshalling | [23929ee3](https://github.com/crawl/crawl/commit/23929ee3b245)
| 11        | Dec 2009 | 0.6a           | ? (change adds Mara) | [9fcc7c41](https://github.com/crawl/crawl/commit/9fcc7c41761e)
| 10        | Dec 2009 | 0.6a           | save compat? (artifacts) | [dffde3bb](https://github.com/crawl/crawl/commit/dffde3bb594a)
| 9         | Dec 2009 | 0.6a           | add tile types | [b03ae0bd](https://github.com/crawl/crawl/commit/b03ae0bd75db)
| 8         | Nov 2009 | 0.6a           | remove divinations | [79c3d7c3](https://github.com/crawl/crawl/commit/79c3d7c39771)
| 7         | Nov 2009 | 0.6a           | ? | [843ae076](https://github.com/crawl/crawl/commit/843ae0761b1d)
| 6         | Jun 2009 | 0.6a           | artifact changes | [2fad0537](https://github.com/crawl/crawl/commit/2fad05374f70)
| 5         | Apr 2008 | 0.4            | save game versioning | [9cc4ae67](https://github.com/crawl/crawl/commit/9cc4ae67d7ab)

**Notes**:

* For a brief window in Nov 2011, the major tag was set to 1 and then 100 for some kind of save compat issue. At least part of this appears to have happened on a branch (portal_branches), but the history appears in master -- I think possibly this was to workaround in case the major tag incremented while a long-running branch was under development. See [7b91fc81](https://github.com/crawl/crawl/commit/7b91fc814655) etc. The tag went directly from 100 to 33 in the history of master, though this may have all been merged at once.
* An initial bump to 32 was reverted by kilobyte in Nov 2010, apparently because it wasn't necessary.
* 23 was skipped due to a server-specific workaround for a bug on CDO (see commit message for [d1767427](https://github.com/crawl/crawl/commit/d1767427b7affeff7902783c3582492a932be4ec) ).
* In the 21 cycle, the location of TAG_MAJOR_VERSION was moved, with a brief life in enum.h;  See [f40f35fa](https://github.com/crawl/crawl/commit/f40f35fa6b8c), [1687dfc5](https://github.com/crawl/crawl/commit/1687dfc5bcf2).
* Prior to the 16 cycle, TAG_MAJOR_VERSION was an enum, rather than a #define.
* The use of `TAG_MAJOR_VERSION` begins in 0.4, with [9cc4ae67](https://github.com/crawl/crawl/commit/9cc4ae67d7ab).

**Some analysis**. Over time, there are three basic trends that seem to have governed major tag increases. First, it seems that some developers have been more willing to increment it than others, just for safety -- current preference is very much for using minor version tags. I suspect that this first trend also corresponds with a growth in institutional knowledge about how to handle save compatibility well (so it may not be fair to blame particular developers, and the table above doesn't). For comparison, there have been **182** minor version tags as of Feb 2017 on major version 34. Second, over time there has been an effort to decouple hard assumptions in the game from save game state, making it much easier to use minor version tags; [b4fa663e](https://github.com/crawl/crawl/commit/b4fa663e1c51bf6e459c572eae2ef6a3e8fecaa5) is a good example of this kind of change. Third, it seems that many of key features that would be really hard to do while preserving save compatibility were roughly stabilized prior to the 34 major version. A good example of this kind of change is the implementation of map layer saving in [af76f809](https://github.com/crawl/crawl/commit/af76f809d3f1679210469c3b194f1b06dc85536b), which brought saves fully in line with the information shown in tiles.

One final factor is probably that if save compatibility is already irrevocably broken once within a single development cycle, there's not so much penalty in breaking it again in that cycle.

**Save-compat failures**. Backwards compatibility is an ideal, and so hasn't always worked out. Sometimes this can be fixed, but if not, would be a plausible reason to increment the major branch tag; looking at fixes of this kind also gives some hints about the institutional knowledge mentioned in the first reason above. Two interesting case studies are the spectral weapon bugs discussed in detail in [|amethyst's blog post on save compatibility](https://crawl.develz.org/wordpress/save-compatibility-in-dcss-2), and the recent case of loading branch_info in 0.19, that I worked on. Both of these fixes involved a minor version increment, but not a major. In this latter bug, under certain conditions (if the player had opened a game and then returned to the main menu) a save could be imported, but would save with the PlaceInfo for a new branch in 0.19, the desolation of salt, set with an incorrect branch number (127). This save would crash on loading thereafter. See [4c9d53b3](https://github.com/crawl/crawl/commit/4c9d53b3c18c7e8ce2f03422e701039eb367dc48) and [ab781cdc](https://github.com/crawl/crawl/commit/ab781cdc92f8be826bd08a95c8cce56eb1172dc3) for the save fixes, and [48079674](https://github.com/crawl/crawl/commit/480796749b9868009da8050bf61395be0c819ae2) for the actual save-compat-breaking bug. An earlier version of this bug had already shown up briefly when depths was introduced, and it was dealt with by simply [disabling](https://github.com/crawl/crawl/commit/7d816dcee34c) and then a week later [reenabling](https://github.com/crawl/crawl/commit/224314a15c3f) an assert that triggered when the PlaceInfo for depths was set incorrectly.

Many more such cases can be found (at least until the next tag increment) by browsing [tags.cc](https://github.com/crawl/crawl/blob/master/crawl-ref/source/tags.cc) for `#if TAG_MAJOR_VERSION == 34` blocks. Here's a sampling of a few more cases, pulled from comments. Some of these are post-hoc, some are preemptive.

 * // From 0.18-a0-273-gf174401 to 0.18-a0-290-gf199c8b, stash search would change the position of items in inventory.
 * // save-compat: if the player was wearing the Ring of Vitality before it turned into an amulet, take it off safely
 * // We briefly had a problem where we sign-extended the old 8-bit item_descriptions on conversion. Fix those up.

       ```c++
       if (th.getMinorVersion() < TAG_MINOR_NEG_IDESC
           && (int)you.item_description[i][j] < 0)
       {
           you.item_description[i][j] &= 0xff;
       }
       ```

 * // enum was accidentally inserted in the middle

       ```c
       if (th.getMinorVersion() < TAG_MINOR_ZIGFIGS
           && item.is_type(OBJ_MISCELLANY, MISC_ZIGGURAT))
       {
           item.sub_type = MISC_PHANTOM_MIRROR;
       }
       ```

**Useful pickaxe commands**:

       git log -G"TAG_MAJOR_VERSION" -- --pickaxe-all tags.h
       git log -G"TAG_MAJOR_VERSION +[0-9]+" -- --pickaxe-all tags.h
       git log -G"TAG_MAJOR_VERSION += +[0-9]+" -- --pickaxe-all tags.h
