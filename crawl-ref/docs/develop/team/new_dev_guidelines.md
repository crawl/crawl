# Guidelines for Development Team Members

This document gives new team members some general guidance for DCSS development as well as links to useful resources.

## Setup

Other team members will help you get the items in the [*new dev checklist*](http://git.develz.org/?p=crawl.git;a=blob;f=crawl-ref/docs/develop/team/new_dev_checklist.txt) covered. Once you’ve been given commit access, configuring git properly so that you can make your first commit is a priority. See the [*git configuration doc*](http://git.develz.org/?p=crawl.git;a=blob;f=crawl-ref/docs/develop/git/config.txt), especially the options about rebasing so you avoid making unnecessary merge commits.

## Team Discussion and Coordination

Most development discussion happens on IRC in the \#\#crawl-dev on Freenode, so it's good to join and participate in that channel. Use the !tell command through Sequell to leave IRC messages to other team or community members you’re working with. The channel is also [*logged*](http://s-z.org/crawl-dev/#), if you’d like to read or search previous discussions.

Although nearly all changes are just discussed on IRC, larger projects can be written up in an email sent to the [*CRD mailing list*](http://sourceforge.net/p/crawl-ref/mailman/crawl-ref-discuss/) or placed on page in the [*Dev Wiki*](https://crawl.develz.org/wiki/doku.php). The Dev Wiki is helpful if you’d like to brainstorm and organize a large list of changes. The CRD mailing list is a way to discuss high-impact changes if you feel extended, organized discussion is necessary.

We also coordinate [*release plans*](https://crawl.develz.org/wiki/doku.php?id=dcss:planning:release_plans) on the Dev Wiki. These plans are most useful at the beginning of each version, when we’re brainstorming what major areas to work on, and closer to release, when we decide which features will make it in the next version. You don’t have to continuously update entries you make in a release plan during the alpha version, but it’s good to update items as you complete them.

## Coding Standards

We value having reasonably correct, well-thought out code that doesn't break other aspects of the game over simply getting in content changes as fast as possible. If you're not sure how to implement something, please ask other team members who have more experience with the codebase. You can use a code pasting service like github gist, dpaste, or pastebin to share code snippets.

## Documentation and Formatting

The [*coding conventions*](http://s-z.org/neil/git/?p=crawl.git;a=blob;f=crawl-ref/docs/develop/coding_conventions.txt) document describes how Crawl’s C++ source code should be formatted. For documenting C++ code, we use [*Doxygen*](http://www.stack.nl/~dimitri/doxygen/index.html) comments and the JavaDoc style, and example of which you can see [*here*](http://s-z.org/neil/git/?p=crawl.git;a=blob;f=crawl-ref/source/ability.cc#l3553). It’s preferred that you document all new functions and data structures in this way, but not required.

Significant changes that affect the gameplay should be documented in the [*changelog*](http://s-z.org/neil/git/?p=crawl.git;a=blob;f=crawl-ref/docs/changelog.txt;hb=HEAD). We try to be brief in these entries, giving players a simple description of the change without technical detail. It’s good to read previous entries or ask a team member if you’re not sure about including an item or how to word it.

## Save compatibility

Each trunk commit creates a new version, and we try hard to not break trunk games when a save is transferred to a new version. Making changes that force users to abandon a game they have in-progress is frustrating for players and creates problems for server administrators as well. There’s a [*document*](http://git.develz.org/?p=crawl.git;a=blob;f=crawl-ref/docs/develop/save_compatibility.txt) in the source tree that discusses basic save compatibility and the technical reasons behind it. In general, if you’re not sure if a change will create a save compatibility issue or how to address it, ask another team member before pushing your changes.

## Release Schedule

Although there isn’t a fixed schedule, releases tend to happen every 6 months and are followed by a two week [*online tournament*](http://dobrazupa.org/tournament/0.19/) based on the released version. The steps for branching and tagging the release in git as well as building the release packages are described in the [*release guide*](https://github.com/crawl/crawl/blob/master/crawl-ref/docs/develop/release/guide.txt). The work for release and tournament is usually done by several people who help out with specific portions of the process from release to release. Ask other team members if you’d like to help out.

## Development Philosophy

As a new developer, it's good to discuss significant gameplay changes with at least a few others before pushing them and to not be too attached to a new idea you may have. Below are some general principles about the development process.

#### Team Cooperation

Team members have varying levels of free time, and their availability fluctuates. It's important to listen when others say they may not have time to critique your idea or double-check your code. Be prepared to ask someone else or the team as a whole in \#\#crawl-dev or on CRD. If your implementation seems well tested and reasonably agreed upon by the rest of the team in your best judgement, go ahead and push it to the repository!

#### Game Design

It's important to play Crawl if you want to design for it. Experiencing the content you create is very helpful in determining whether that content is enjoyable and creates the intended game-play experience. Playing a variety of characters in Crawl also helps you understand how the game does and does not succeed in being highly replayable, with an emphasis on combat, and where the player is generally not rewarded for tedious game-play. You can also help us uncover bugs and problems with existing code, and stay informed as to the current state of Crawl.

#### Player Feedback

In addition to play-testing, it’s good to watch players through WebTiles, via console ttyrecs, videos and other online recordings . Likewise, it can be useful to see what players think about certain features by reading online discussion on [*Tavern*](https://crawl.develz.org/tavern/), Reddit (see [*/r/dcss*](http://www.reddit.com/r/dcss/) and [*/r/roguelikes*](http://www.reddit.com/r/roguelikes/)), and other online discussion sites. However it’s important that player praise or criticism is always taken with a grain of salt. If in doubt, connect with other developers -- \#\#crawl-dev is always good for that.
