# Release Guide

The following is a step-by-step guide for how to proceed through the releasing
process. For minor releases (0.X.y), steps 1 and 2 have already been done, so
for those the list begins with step 3.

## Release timing and prerequisites

We currently have the release divided into two parts, the feature freeze and
the final release. We usually have the feature freeze start about two weeks
before the release date, with the tournament starting the Friday of the week
of the release.

When branching and tagging the beta release, you need to have two commits
ready:

* A commit to tag as `0.X-b1`, where `0.X` is a major release number like 0.31.
  This should be the first commit of the release branch.
* A subsequent commit in trunk to tag as `0.{X+1}-a0`, where `0.{X+1}` is the
  next major release number, like 0.34. The specific contents of this commit
  don't matter, but it must be a distinct, subsequent commit from the one
  tagged with `0.X-b1`.

## 1. Branch master into the release branch and tag it

The release branch should ideally be made at least a week before the
tournament, to give server admins time to set up the new branch. To make the
new branch, create the branch with the *first commit* as `HEAD`:

```bash
git branch stone_soup-0.X
git push origin stone_soup-0.X
```

The beta and alpha tags must be annotated tags made with `git tag -a` that
have commit messages, and those messages should follow a specific format. To
make the beta tag, have the `stone_soup-0.X` branch checked out or otherwise
have the *first commit* again as `HEAD`, and do:

```bash
git tag -a 0.X-b1
git push origin 0.X-b1
```

Example beta tag commit message:
> 0.33-b1: The 0.33 release branch

### Sequell and the beta tag

Note that, despite the release branch having a `0.X-b1` tag, Sequell will show
games played on it as having a `cv` field of `0.X-a`, the same value as games
played in trunk before any tagging. This is intended, so we only have two
different `cv` values to deal with for Sequell queries within the same
version. After the final release tag is made, subsequent games in the release
branch will have a `cv` of `0.X`.

## 2. Make the alpha tag for the next trunk version

Now you should have `master` checked out and be at the *second commit*, which
should also already be pushed to `origin/master`. To make the alpha tag:

```bash
git tag -a 0.{X+1}-a0
git push origin 0.{X+1}-a0
```

Example alpha tag commit message:
> 0.34-a0: Open the 0.34 season.

## 3. Update servers

Contact the server admins to ask them to add the new version, and preferably to
make it the default until the end of the tournament. This will increase the
amount of play-testing the branch gets before release. The current list of
servers, their admin, and preferred point of contact.

* CAO: gammafunk or advil on IRC or the dev Discord.
* CBR2: ZureaL on the dev Discord
* CDI: gammafunk on IRC or the dev Discord.
* CDO: Recently CDO has not been installing stable branches and just hosts
  trunk. It still hosts the crawl website and tournament, among other things.
* CNC: ASCIIPhilia on the dev Discord.
* CUE: TZer0, on the dev Discord or the CUE Discord.
* CXC: melvinkitnick on the dev Discord or the CXC Discord.
* LLD: dplusplus on the Roguelikes Discord or the LLD Discord.

Server admins may not have the new release branch set for daily updates. Hence
it may be necessary to ask them to rebuild the stable branch shortly before
tournament.

## 4. Fix bugs in the beta branch until release time

During the features freeze, no major new features should be added to either the
release branch nor trunk. In fact, the release branch and trunk should remain
synced during feature freeze, with all commits cherry-picked between the two.
Any major new features for the next trunk version can go to a separate branch,
but shouldn't be pushed to trunk itself. This way the player focus is on
testing the release version.

To check for candidates for cherry-picking (with `master` checked out):

```
git cherry -v origin/stone_soup.0.X
```

To actually take them (with the release branch checked out):

```
git cherry-pick -x <commit hash>
```

## 5. Update the changelog and other documentation files

The changelog should be finalized to include all notable changes for the
release version and include a set of release highlights at the top. The
release highlights should be a short list of the most important content
changes summarized very succinctly. Past changelogs entries provide good
examples.

Before release, it's good to go through all player-facing documentation,
especially the Crawl manual, and check that the tutorials are functioning and
up to date. The [debian
changelog](https://github.com/crawl/crawl/blob/master/crawl-ref/source/debian/changelog)
should also be updated; see the [debian release guide](https://github.com/crawl/crawl/blob/master/crawl-ref/docs/develop/release/debian.md#21-update-and-commit-the-debian-changelog)
for details.

## 6. Tag and create the release

In the release branch you're about to tag:

```
   git tag -a 0.X.y
   git push --tags
```

Then, visit the GitHub [releases page](https://github.com/crawl/crawl/releases)
a create a release from this tag. For the release message, simply paste the
full changelog contents for the release into the message box. It will be
interpreted as markdown. Once you create the release, GitHub Actions will
automatically upload Windows, macOS, and Linux Debian and AppImage builds as
assets.

## 7. Update the download page on CDO

There's a [repository](https://github.com/crawl/dcss-website) that manages the
web files for non-wordpress pages on CDO like the splash and download pages.
Update the [download
page](https://github.com/crawl/dcss-website/blob/master/site/download.htm) to
have current links for the release and downloads. It's best to update the files
from a local checkout of the repo, push a commit for the change to the
github repo, pull the commit on the clone on CDO at `~/dcss-website`, and then
copy the updated `~/dcss-website/site/download.htm` page from the repo to the
live `~/website` directory on CDO.

## 8. Install the debian packages on CDO

For installing the debian packages into the CDO PPA; see the [debian release
guide](https://github.com/crawl/crawl/blob/master/crawl-ref/docs/develop/release/debian.md#4-install-files-into-the-official-crawl-ppa)
for details.

## 9. Announce the release

Write a release announcement to the CDO blog. The post should contain a brief
summary of the changelog release highlights and links to the release and the
download page. Tag it  and post it on [Tavern](https://tavern.dcss.io/), the
Roguelikes Discord, and the r/dcss Reddit.
