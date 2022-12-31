# Guide for contributing to crawl

We welcome and appreciate community contributions, and contributions that meet
our standards are accepted for all areas of the game. You can fix typos and
standing bugs; optimize a process; submit a new vault; create a new (or
improved) tile for a monster, item, or dungeon feature; create a new item or
spell; or even create a new species, monster, or god. This document is a guide
to the process of making a contribution and has pointers to use during
development and discussing.

Please note that since crawl is an entirely volunteer project, and the
development team's available time fluctuates. So the time it may take us to
evaluate contributions will vary. In addition, not every contribution will be
accepted. Also keep in mind that the more significant your contribution is, the
more scrutiny it will receive! Small bugfixes and minor vaults are more likely
to be approved than not, but a new species or new god will undergo a thorough
review process by many members of the dev team if it seems if it will be merged
into the game.

## Talking about game development

Most discussion happens nowadays via IRC at the `#crawl-dev` channel on Libera.
This is where casual discussion and review happens. If you have a quick
question about design or coding or whatnot, this is where you'll get the best
turnaround time.

Users with +v are members of the devteam and have commit access. Developers
often discuss what they are working on on the channel and request comments
before commits. Therefore the channel is logged to archive discussion and
decisions made there. The archive can be found here:

    http://s-z.org/crawl-dev/

### Writing a proposal for gameplay-related contributions

For proposals that would have a significant impact on gameplay, it's good to
get feedback and discussion from experienced crawl players. One place to start
is the [Game Design Discussion
board](https://crawl.develz.org/tavern/viewforum.php?f=8) (GDD) on the official
Tavern forums. Please follow the GDD discussion guidelines posted at the top of
that forum.

For planning and brainstorming, there is a [Doku development
wiki](https://crawl.develz.org/wiki/doku.php) where anyone can register to
create and edit wiki pages. This wiki is more useful if you're working on a
larger project that's in need of outside ideas

An alternative to the Doku wiki is github wiki system. You can fork the crawl
repository if you haven't done so already and create wiki pages in that fork.
By default, github users who aren't collaborators for your repo can't edit wiki
pages. You can allow other github users to edit your repository's wiki without
allowing them commit access to the repository itself by going to the Settings
tab of the repository and unchecking the box "Restrict editing to collaborators
only". Finally, you can also add specific github users (such as a dev you are
working with) to your repo's list of collaborators, which also grants them
commit access in your repo.

## Development documentation and references

Before you start development, you'll want to look through the game's
documentation (in [docs/develop/](/crawl-ref/docs/develop), where this file is
located) for guidelines, as well as compare your ideas and work against previous
changes to Crawl and current Crawl features.

If you're coding, then you'll want to read the setup process outlined in
[docs/develop/git/quickstart.txt](/crawl-ref/docs/develop/git/quickstart.txt)
as well as set up an IDE or editor to make your changes to the Crawl codebase.
Crawl is coded primarily in C++, with Lua used for level layout and vault
definitions as well as for some aspects of controlling the client. The
WebTiles server is coded in python3 and uses the Tornado framework. The
WebTiles browser client is written in JavaScript using the jQuery and other JS
libraries. The local Tiles version uses the SDL2 graphics library.

If you're doing tiles art, or splash screen art, then it's recommended to look
at previous artwork and try to match the general style presented there. This is
not a hard-and-fast rule, and clear improvements are always welcome.

If you have questions for how to progress further, then feel free to pop into
`#crawl-dev` and ask for help. Responses are reliant on dev availability and
knowledge/expertise.

## Submitting a pull request

The normal way to submit a contributing to crawl is by making Pull Request (PR)
on github. The technical parts of this process are fairly standard for any
github-hosted project. We also will accept patches uploaded to mantis, but
would prefer this to be limited to small changes like vaults or artwork.

### A typical example of the process

1. You clone the crawl repository and make your changes in a branch on your
   cloned repository.
    * Observe the [code style guidelines](coding_conventions.md) and [commit
      message style guidelines](coding_conventions.md#1---commit-conventions)
      (72 char width, line between title and body, bug # or reporter in title).
    * Include a commit message with meaningful content for every commit. It's
      fine if a PR comment duplicates this, but the priority for explaining the
      changes should be in the commit messages themselves.
2. You open a PR in the main repository based on your branch.
3. Some member(s) of the devteam reviews the commit, probably asking some
   questions and making some suggestions for changes. This happens at the pace
   of devteam availability, which can fluctuate. Be prepared to check in with devs
   from time to time to ask about your PR.
4. You make some changes to your branch and push, which will automatically
   update the PR.
    * Devteam members won't make changes in a PR branch, and force-pushing to
      your fork's branch is fine. However, in some cases where changes were
      requested it can be useful for us to see the commit sequence (and some
      devteam members prefer this for newer contributors). There's no
      requirement to rebase changes.
    * In branches we generally prefer rebasing on master rather than merge
      commits. However we can rebase your branch ourselves when merging.
    * Steps 3-4 may repeat.
5. If all goes well, a devteam member merges the PR. This will typically
   involve a rebase, and the devteam member may tweak some details of the
   commit(s) at this time, and potentially squash commits.

## Some pointers on making contributions

* It is often a good idea to chat about even small changes with some devteam
  members, especially if you are new to the dcss codebase or to game design.
  The best place for this is in the #crawl-dev IRC channel on Libera.
* PRs that have good commit messages and conform to style guidelines tend to
  get merged faster, all things being equal. A *very common* mistake is to
  put a lot of text in the PR first comment, and leave the commit message blank.
  Usually, this will require a devteam member to manually copy information
  from the PR and reformat for a commit message, slowing the process.
* PRs that fix bugs are very likely to get merged in some form (though you may
  receive suggestions for alternative strategies for fixing the bug).
* PRs that change gameplay in particular need to be carefully thought out, and
  ideally, discussed with members of the devteam ahead of time.
    * Think carefully about what you want the change to accomplish, and provide
      justification in the commit message(s).
    * Think carefully about whether a change might lead to something surprising
      being optimal.
    * Think carefully about how the change will be received by both extremely
      strong players and by new players.
    * If the change is in response to a strong opinion or reaction that you
      have to some aspect of crawl, consider whether your opinion is likely to
      be universal or obvious. It's best if you can be dispassionate about proposed
      changes.

Community members often talk about "the devteam" as if it's a single entity
with one specific set of plans, beliefs, aesthetics, etc. This view is not
correct, and so you should be aware that one response you get to a proposal (be
it positive or negative) doesn't necessarily indicate the views of the whole
devteam, or preclude an opposite response from some other devteam member.

### A note on major contributions

We do welcome major contributions that introduce something substantially new
(e.g. gods, species, UI overhaul, etc), and many contributed patches along
these lines have made it in to the game. However, many haven't, and the bar for
getting such a change into a stable release of DCSS is high. If you would like
to make such a change, you should go into it prepared for the possibility that
it won't make it, and that if it does, it will make it in a substantially
revised (perhaps unrecognizable) form. You should also be prepared for the
contribution to involve a process, with potentially many revision cycles.

If your goal is getting something into stable (see below for a definition of
"stable"):
* We generally recommend against a new god, species, role, branch, or anything
  of this scope being your first contribution.
* You will need to be responsive in a positive way to feedback, including
  critical feedback.
* You will need to be prepared for the latter parts of the acceptance process
  being rather detail-oriented, making sure all the UI elements work as
  expected, checking all the special cases you can think of, etc. If you don't
  do this part some devteam member would need to, which will substantially slow,
  or even prevent, acceptance into the game.
* Major contributions will very much be tied to the alpha/beta/release cycle
  for the game: they are very unlikely to be accepted close to a release target.
  (This can go for smaller contributions as well, if they are likely to require
  a fair amount of testing for one reason or another.)

N.b. this is open-source software, your goal doesn't have to be getting a
change into stable! Many devteam members are happy to talk to fork developers,
or people just developing something for fun.

#### Definitions: branches, experimentals, trunk, stable

One potential path for a new major-ish contribution (and some contributions
from inside the devteam) is branch -> experimental -> trunk -> stable.

**Branch**: a git branch that has some set of proposed changes on it. A github
PR is a special kind of branch.

**Experimental**: sometimes server operators are willing to install a branch in
the repository as an additional playable version of the game in order to test
out a large feature. Games in experimentals are indexed by Sequell and do get a
fair amount of play, but much less than trunk. All that is needed to have an
experimental version is a branch that doesn't crash too much and the goodwill
of a server operator. Experimental branches are generally used for developing
large features that have some initial acceptance from the devteam but that need
extensive play-testing and would be invasive to test in trunk due to not being
in a finalized state.

**Trunk**: the current unstable testing ground for new features or changes to
crawl; if you have a PR merged it will go into trunk. Many online players play
trunk, so something being tested in trunk receives wide exposure. A feature
being in trunk is *not* a guarantee that it will make it into stable, and
anything complicated enough will usually need iteration in trunk. Sometimes
features that do make it are trunk-only for multiple stable release cycles.

**Stable**: stable versions of crawl are released on a ~6month cycle,
coinciding with a tournament; each stable release takes the form of a branch
off of trunk at the time of release, and will receive mainly bugfix patches
from that point. Online players play both stable and trunk, and offline
players predominantly play stable.

### After your contribution has been accepted

Firstly, thank you for helping to improve and expand Crawl! After your
patch is included, then it becomes part of Crawl, and as such will come under
the jurisdiction of the dev team. Your contribution may be altered, sometimes
even significantly, from what you originally envisioned for it. Sometimes this
means a total rewrite of the code, other times it means an overhaul of how it
works, and occasionally it might end up being removed one day.

This is part of the natural process of developing and maintaining Crawl, to keep
its design healthy and long-lasting. It won't be a reflection upon you or your
ideas if it is altered.

Of course, most of the time, especially when they were originally in line with
general Crawl design philosophy, contributions and additions to Crawl won't be
significantly altered.
