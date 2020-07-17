# Preparing the DCSS Debian Package

This guide covers making the packages for a debian system like Debian or
Ubuntu and installing these into the CDO PPA,
https://crawl.develz.org/debian/. The goal is to generate packages that
install on both Debian and Ubuntu for the most common architectures.
Downstream distributions do package crawl as well, but often lag behind
relative to current crawl versions, and the CDO PPA is updated with every
release. We currently produce packages for both i386 and amd64 architectures
using debian (either stable or oldstable), which usually allows installing on
Ubuntu as well. This guide has steps for building these using cowbuilder and
pbuilder, based on the debian directory in crawl-ref/source/debian of the
crawl source repository.

The Debian maintainer's guide is a good resource for working with debian
packages:

https://www.debian.org/doc/manuals/maint-guide/index.en.html

## 1. Prerequisites and setup

Basics:

1. A Debian system of some kind. One recommended option is to use a docker
   container that is specific to this purpose. This section walks you through
   installing the main packages (cowbuilder, pbuilder) used for building the
   packages.
2. Login access to crawl.develz.org, in order to install into the PPA.

The cowbuilder and pbuilderrc can be set up once and re-used for each DCSS
release. The cowbuilder directory should be updated before each release.

See the following documentation if you have trouble with cowbuilder or
pbuilder during any of these steps:

* https://wiki.debian.org/cowbuilder
* https://wiki.debian.org/PbuilderTricks

### 1.1 Set up your pbuilderrc

We use the cowbuilder program to create the copy-on-write chroot directories,
and use pbuilder to build the packages in the chroot. A `pbuilderrc` file is
needed to tell pbuilder to use the cowbuilder system and to set downstream
shell variables. To use the example pbuilderrc file in this directory, copy it
to `~/.pbuilderrc` or (as root) to `/etc/pbuilderrc` after making any
modifications.

You can edit this file to set the variables `DEBFULLNAME` and `DEBEMAIL` to your
name and email. You can also set and export these variable directly in your
shell.

In the example pbuilderrc, all pbuilder-related data go in
`/var/cache/pbuilder`. If you need to change this location, you'll need to
modify at least `BASEPATH`, `BUILDPLACE`, `BUILDRESULT`, and `APTCACHE`.

### 1.2 Install the cowbuilder and pbuilder packages

This should get you the necessary packages:

    sudo apt-get install cowbuilder debhelper

### 1.3 Create a .cow chroot directory for each architecture.

You'll need a cowbuilder chroot directory for each architecture you want to
build. If you're using the example pbuilderrc, we use the scheme
`DIST-ARCH.cow`. For example, when building based on debian oldstable, we'd use:

    /var/cache/pbuilder/oldstable-amd64.cow
    /var/cache/pbuilder/oldstable-i386.cow

To create these, run the following:

    sudo OS=debian DIST=oldstable ARCH=amd64 cowbuilder --create \
      --basepath /var/cache/pbuilder/oldstable-amd64.cow
    sudo OS=debian DIST=oldstable ARCH=i386 cowbuilder --create \
      --basepath /var/cache/pbuilder/oldstable-i386.cow

In principle you can leave some of these environment variables out where they
match your current system, but it is safest not to simplify. In order for
these to run succesfully in docker, you will need to be running the docker
image with `--privileged`.

We build the debs against either debian stable, or debian oldstable. Basically,
choose the oldest version that still works, in order to preserve compatibility
with Ubuntu.

This chroot directory needs to be created only once for each OS/ARCH/DIST
combination on which you what to build the package. If It doesn't need to be
recreated if you're only building a different DCSS version, but if you're using
a different value for any of `OS`, `ARCH`, or `DIST`, you'll need to create a
corresponding .cow chroot.

## 2 Steps needed before release and before building the packages

The changelog update described in 2.1 should be done before you make the
release tag. If you forget, you can apply the changelog changes in the copy of
the debian directory you make in section 3.2.

### 2.1 Update and commit the Debian changelog

To update the Debian changelog, add an entry at the top of this file in the
following format.

    crawl (2:VERSION-1) unstable; urgency=low

      * Major bugfix release
     -- devname <devemail>  TIMESTAMP

You can copy the previous entry, update the version, and use output from the
`date' command to update the timestamp. A command like the following will
give you a valid timestamp:

    date +'%a, %d %b %Y %R:%S %z'

The entire entry must match the format of previous entries; note the lines
following the first `crawl` line have leading spaces. Incorrect formatting
can cause debuild and hence pbuilder to fail.

Once the file is updated, commit the change to the repository so that it's
included in the release version tag.

### 2.2 Updating the cow chroots

It's good to update your cow chroot directory with the latest security/bugfix
updates to its packages. For the above example of using debian oldstable, this
would involve:

    sudo OS=debian DIST=oldstable ARCH=amd64 cowbuilder --update \
      --basepath /var/cache/pbuilder/oldstable-amd64.cow
    sudo OS=debian DIST=oldstable ARCH=i386  cowbuilder --update \
      --basepath /var/cache/pbuilder/oldstable-i386.cow

## 3. Making the Debian packages

At this point the release should be tagged.

### 3.1 Make a copy of the of the source packages and extract

The source packages should be made using the `package-source` target. Run
the following from the source directory if you haven't yet:

    make package-source

This will make several files in the toplevel repo dir, but you specifically
need the `stone_soup-VERSION-nodeps.tar.xz` file. Copy this to a location where
you'd like to prepare the packages, then extract the source directory and
rename the original file. Using version 0.17 as an example and with
`~/crawl-deb` as the staging directory:

    mkdir -p ~/crawl-deb
    cp stone_soup-0.17.0-nodeps.tar.xz ~/crawl-deb
    cd ~/crawl-deb
    tar Jxf stone_soup-0.17.0-nodeps.tar.xz
    mv stone_soup-0.17.0 crawl-0.17.0
    mv stone_soup-0.17.0-nodeps.tar.xz crawl_0.17.0.orig.tar.xz

Note that the name formats of `crawl-VERSION` and `crawl_VERSION.orig.EXT` for
the source directory and archive are specifically looked for by pbuilder. If
you receive errors, check that your source directory and archive follow this
format, using `crawl-` as a prefix for the directory but `crawl_` as a
prefix for the source archive.

### 3.2 Copy and update the debian directory in the source directory

We need the `crawl-ref/source/debian` directory to be at the top level to
build the package. Using 0.17.0 as an example, and assuming we're already in
our staging directory with the unpacked source:

    cd crawl-0.17.0
    cp -r source/debian .

### 3.3 Build the packages

Assuming your pbuilderrc is based on the example one in this directory, you
need to set some of the shell variables `ARCH`, `DIST`, and `OS` downstream
variables. The pbuilderrc uses these to build the packages for the
architectures (e.g. `i386` or `amd64`) you want and against the distribution you
want (e.g. `stable` or `oldstable` for Debian).

Run `pdebuild` from the `crawl-VERSION` source directory you made above. As with
building the cows, the safest option is to explicitly specify the `OS`, `DIST`,
and `ARCH` for each build, though in principle you can leave off options
matching your current system.

    sudo OS=debian DIST=oldstable ARCH=amd64 pdebuild
    sudo OS=debian DIST=oldstable ARCH=i386 pdebuild

Once the package building is finished, the results will be in
`/var/cache/pbuilder/result/`, if you're using the example pbuilderrc.

At this point, it may be a good idea to check the dependencies of the package
you just built. This will head off OS version mistakes, and allow you to
identify potential incompatibilities before release. For example, it is worth
checking that currently common versions of Ubuntu meet the SDL2 requirements
imposed by your debs (and high version requirements can also be a sign that
you built against the wrong OS or version somehow). You can use `dpkg -I` to
do this, for example:

    dpkg -I /var/cache/pbuilder/result/crawl-tiles_0.24.0-1_amd64.deb

This will show a result something like (abbreviated):

    Package: crawl-tiles
    Source: crawl
    Version: 2:0.24.0-1
    Architecture: amd64
    Maintainer: the DCSS Development Team <crawl-ref-discuss@lists.sourceforge.net>
    Installed-Size: 10205
    Depends: libc6 (>= 2.14), libfreetype6 (>= 2.2.1), libgcc1 (>= 1:3.4), libgl1-mesa-glx | libgl1, libglu1-mesa | libglu1, liblua5.1-0, libsdl2-2.0-0 (>= 2.0.4), libsdl2-image-2.0-0 (>= 2.0.1), libsqlite3-0 (>= 3.5.9), libstdc++6 (>= 5.2), zlib1g (>= 1:1.1.4), crawl-common (= 2:0.24.0-1), crawl-tiles-data (= 2:0.24.0-1), fonts-dejavu-core

These dependencies (built against debian oldstable at the time) are reasonable,
because Ubuntu xenial (at the time the most common LTS version) provides
libsdl2 2.0.4.

## 4. Install files into the official crawl PPA

You'll need to upload all the files produced for this version in
`/var/cache/pbuilder/result` (not just the deb and dsc files) to CDO under the
`crawl` account. To keep the home directory clean, create and use a staging
directory `~/upload/VERSION/deb-files/`.

### 4.1 Install a new repo component (major release only)

For point releases, this step should be skipped.

For major releases, we make a new release component for all releases of that
version. If you're logged into CDO, edit the `deb/conf/distributions` file,
adding the new version in the "Components:" field before the final entry for
"trunk", e.g. for 0.17:

    Components: 0.10 0.11 0.12 0.13 0.14 0.15 0.16 0.17 trunk

### 4.2 Install the debian packages into the repository

Install the .deb files and the .dsc file using `reprepro`. An example using 0.17
and `~/upload/0.17.0/deb-files` as a staging directory:

    cd ~/deb
    for i in ../upload/0.17.0/deb-files/*.deb
        do reprepro -C 0.17 includedeb crawl "$i"; done
    for i in ../upload/0.17.0/deb-files/*.dsc
        do reprepro -C 0.17 includedsc crawl "$i"; done

Note that the other files produced by pdebuild in `/var/cache/pbuilder/result`
(section 3.3) must be present in the same directory as the dsc file you
install.

Note: If you messed something up and need to upload a new build of packages
for a version already installed in the repo, you can remove the installed
debs with a command like the following (run from ~/deb):

    reprepro -C 0.17 remove crawl crawl crawl-common crawl-tiles crawl-tiles-data crawl-tiles-dbgsym crawl-dbgsym

Test that the repository packages are working by following the apt instructions
on the download page to install and run them.

### 4.3 Update the download page

The version number in the Linux example command to add the repository URL
should be updated. See release.txt for details on keeping the CDO website in
sync with the website repository. (If you are following the release guide
yourself, there is a point where you will update the current version number
for all downloads.)

At this point, the repository should be working and the packages ready for
users to install with `apt`.
