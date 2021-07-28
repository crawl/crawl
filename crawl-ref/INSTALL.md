# Install instructions for Dungeon Crawl Stone Soup (DCSS)

## Contents

* [Getting DCSS To Run](#getting-dcss-to-run)
* [Compiling](#compiling)
  * [Ubuntu / Debian](#ubuntu--debian)
  * [Fedora](#fedora)
  * [Other Linux / Unix](#other-linux--unix)
  * [AppImage](#appimage)
  * [macOS](#macOS)
  * [Windows](#windows)
    * [MSYS2 (Recommended)](#msys2-recommended)
    * [Windows Subsystem for Linux (WSL)](#windows-subsystem-for-linux-wsl)
    * [Visual Studio](#visual-studio)
* [Advanced](#advanced)
  * [ccache](#ccache)
  * [Installing For All Users](#installing-for-all-users)
  * [Desktop files and AppStream metadata](#desktop-files-and-appstream-metadata)
  * [.des Level Compiler](#des-level-compiler)
  * [Code Coverage](#code-coverage)
  * [Lua](#lua)
  * [PCRE](#pcre)
  * [Unicode](#unicode)
* [Getting Help](#getting-help)

## Getting DCSS To Run

You can download prebuilt versions of DCSS from the
[DCSS homepage](https://crawl.develz.org/download.htm).

If you're having any trouble running these versions, try asking for help in
[any of the community forums detailed in the README](../README.md#community).

The rest of this file deals with compiling DCSS from source.

## Compiling

Here's the basic workflow. Please read the section for your OS below for more
detailed instructions.

```sh
# Clone the repository
git clone https://github.com/crawl/crawl.git
cd crawl

# Either install dependencies from your OS package manager (eg apt, yum)
apt install ...
# or use DCSS's packaged dependencies
git submodule update --init

# Build DCSS (remove TILES=y for console mode)
cd crawl-ref/source
make -j4 TILES=y

# Play DCSS by running the compiled binary
./crawl
```

### Packaged Dependencies

DCSS uses Lua, SDL, SQLite and several other third party packages. Generally
you should use the versions supplied by your OS's package manager. If that's
not possible, you can use the versions packaged with DCSS.

To use packaged dependencies:

1. Clone the repository with Git (you can't use a tarball - the dependencies
   use Git submodules).
2. Run `git submodule update --init` in the repository.
3. Compile as per above.

### Ubuntu / Debian

These instructions may work for other DPKG-based distros.

```sh
# python-is-python3 is required for Ubuntu 20.04 and newer
sudo apt install build-essential libncursesw5-dev bison flex liblua5.1-0-dev \
libsqlite3-dev libz-dev pkg-config python3-yaml binutils-gold python-is-python3

# Dependencies for tiles builds
sudo apt install libsdl2-image-dev libsdl2-mixer-dev libsdl2-dev \
libfreetype6-dev libpng-dev fonts-dejavu-core advancecomp pngcrush
```

Then follow [the above compilation steps](#compiling).

### Fedora

These instructions may work for other RPM-based distros.

```sh
sudo dnf install gcc gcc-c++ make bison flex ncurses-devel compat-lua-devel \
sqlite-devel zlib-devel pkgconfig python3-yaml

# Dependencies for tiles builds:
sudo dnf install SDL2-devel SDL2_image-devel libpng-devel freetype-devel \
dejavu-sans-fonts dejavu-sans-mono-fonts advancecomp pngcrush
```

Then follow [the above compilation steps](#compiling).

## Other Linux / Unix

You need the following dependencies:

* GNU make
* gcc / clang
* perl
* pkg-config
* Python 3 and PyYAML
* libncurses
* flex / bison (optional)

You can install these dependencies from your OS package manager, or use DCSS's
packaged versions (as described in [Packaged
Dependencies](#packaged-dependencies) above):

* lua 5.1
* sqlite
* zlib
* pcre
* zlib
* freetype (tiles builds only)
* DejaVu fonts (tiles builds only)
* SDL2 (tiles builds only)
* SDL2_image (tiles builds only)
* libpng (tiles builds only)

Then follow [the above compilation steps](#compiling).

## AppImage

When building for Linux targets, you can easily create an AppImage with the
help of the `linuxdeploy` tool.

1. [Download the linuxdeploy AppImage](
   https://github.com/linuxdeploy/linuxdeploy/releases)

2. Make it executable.

    ```sh
    chmod +x /path/to/linuxdeploy.AppImage
    ```

3. Follow [the above compilation steps](#compiling) and, when running `make`,
   include the `appimage` target and the path to `linuxdeploy` in the
   `LINUXDEPLOY` parameter.
    
    ```sh
    # console build
    make LINUXDEPLOY=/path/to/linuxdeploy.AppImage appimage
    # tiles build
    make TILES=y LINUXDEPLOY=/path/to/linuxdeploy.AppImage appimage
    ```

## macOS

1. Before building on macOS, you need a working copy of Xcode and the
   associated command line tools.
    1. Install Xcode from the App Store
    2. Open Xcode and say yes if you are prompted to install optional developer
       tools. (You can quit Xcode after this completes.)
    3. Run `xcode-select --install` in a Terminal

2. You will also need to install DCSS's bundled dependencies:

    ```sh
    cd crawl
    git submodule update --init
    ```

3. And install PyYAML:

    ```sh
    pip install pyyaml
    ```

3. If you want to build a macOS application, add `mac-app-tiles` to your make
   command, eg: `make -j4 mac-app-tiles TILES=y`. This will create an application in
   `mac-app-zips/` of the source directory.

Then follow [the above compilation steps](#compiling).

## Windows

### MSYS2 (Recommended)

This is the only currently recommended process for building DCSS on windows.
It is also possible to cross-compile windows builds; see the release guide
for instructions.

MSYS2 is a Cygwin-derived software distro for Windows that can build
native applications; you will interact with the build process in a unix-like
shell interface. You can download the MSYS2 installer from the
[MSYS2 github page](https://msys2.github.io/)

You generally want to install the 64-bit version of MSYS2 unless you have a
specific reason to build a 32-bit version of crawl. Follow all of the steps you
see on that page to install MSYS2, but please read the additional notes below.
In particular, when starting the MSYS2 Shell, be sure to run the 64-bit MinGW
version of the MSYS2 shell and *not* the version labeled 'MSYS2 MSYS' or the
32-bit version.

The installer will put all MSYS2/MinGW files into a folder of your choice,
which defaults to `C:\msys64`. If you have crawl-related work files from
other directories that you'd like to carry over, you can copy them into
`C:\msys64\home\<Username>`, where `<Username>` is your Windows username. This
is the path to your MSYS2 home directory. From the MSYS2 shell you can always
get back to here with `cd ~`.

After the installer finishes, start the MSYS2 shell ("MSYS2 MinGW 64-bit")
and do steps 5-6 from the MSYS2 install instructions in order to update your
installation. These steps are repeated here:

    5. At the command line, run `pacman -Syu` to update the core packages.
       Then restart your MSYS2 shell. (The installer will force you to restart.)

    6. At the command line, run `pacman -Su` to update all packages.

After MSYS2 is fully installed and updated, follow steps below to install
development packages and compile DCSS. The commands shown below should be run
from within the MSYS2 Shell.

1. To install git and the base development packages, run:

    ```sh
    pacman -S base-devel git
    ```

    Accept the default action to install all packages in base-devel, and say yes
    to any questions about installing packages or removing packages due to
    conflicts.

2. To install the mingw-w64 GCC toolchain for your system, run:

    ```sh
    pacman -S mingw-w64-x86_64-toolchain
    ```

3. At this point on current MSYS2 versions, your core development environment
  for building windows binaries within MSYS2 should be complete. You can test
  the toolchain by running:

    ```sh
    gcc -v
    ```

    If this works, you're nearly all set. If it doesn't, you may be an an older
    version of MSYS2 and need to manually add the newly installed toolchain to
    your path. To do so, run the following line, either at the command line (for
    that shell instance only) or in the file `~/.bashrc` to make it permanent:

    ```sh
    export PATH=$PATH:/mingw64/bin
    ```

    (However, you may want to consider just upgrading MSYS2 at this point.)

4. There is one more package dependency needed, the python package PyYAML. This
  can be installed either via the MSYS2 package or (if you know what you're
  doing) via pip.

    ```sh
    pacman -S mingw-w64-x86_64-python-yaml
    # or
    pacman -S mingw-w64-x86_64-python-pip
    pip install pyyaml
    ```

    You can verify PyYAML is installed by running something like:
    ```sh
    python3 -c "import yaml"
    ```

    If this gives an error, something went wrong with the installation. One
    possibility at this point is that you have multiple conflicting python
    versions installed; recent versions of MSYS2 can have this issue. First,
    be sure you are using `python3` (and _not_ just `python`), and if this
    doesn't work, try uninstalling the package named `python`. This may require
    you to also uninstall other packages that are part of the `base-devel`
    group, but they aren't needed for developing with python.

5. To get the DCSS source, follow the steps in the [Getting The
   Source](#getting-the-source) section above to clone DCSS into your MSYS2
   home directory. We recommend using the MSYS2-installed version of git for
   these steps. In brief:

    1. `cd` to the location where you would like the crawl repository to be. It
       will clone into a folder named `crawl`. Your home directory (`cd ~`) is
       a reasonable choice.
    2. Run `git clone https://github.com/crawl/crawl.git`.
    3. Run `cd crawl/crawl-ref/source`.
    4. Run `git submodule update --init`.

6. Build DCSS by simply running:

    ```sh
    # for the console build:
    make
    # or, for the tiles build:
    make TILES=y
    ```

    If you want a debug build, add the target `debug` to the above commands (eg
    `make debug TILES=y`).

7. When the build process finishes, you can run crawl.exe directly from the
   source directory in the MSYS2 shell. For Tiles, type `./crawl.exe`, and for
   console, type `start crawl`, which will open DCSS in a new command.exe
   window (the Windows version of DCSS requires a command.exe shell and will
   not run in an MSYS2 shell). Both versions can also be started by
   double-clicking `crawl.exe` using the file explorer.

8. If you want to build the installer or zipped packages instead,
   you need to install zip and nsis:

    ```sh
    pacman -S zip
    # and
    pacman -S mingw-w64-x86_64-nsis
    ```

    Then build by running:

    ```sh
    # installer
    make package-windows-installer
    # zips
    make package-windows-zips
    ```

### Windows Subsystem for Linux (WSL)

These instructions have been successfully tested with Ubuntu only.

1. [Follow the instructions on Microsoft's
   website](https://docs.microsoft.com/en-us/windows/wsl/install-win10) at to
   set up the Windows Subsystem for Linux.

2. Follow the [Ubuntu instructions](#ubuntu--debian) to build DCSS.

3. Console and Webtiles will work without any further configuration. To get SDL
   tiles to work, you need an X server running on Windows; Xming is an option
   that has worked in the past. If you run into an error about not finding a
   matching GLX visual, try running:

    ```sh
    export SDL_VIDEO_X11_VISUALID=
    ```

### Visual Studio

This build process is currently unsupported, and unlikely to be straightforward
in versions of MSVC besides those explicitly mentioned here.

This build is tested on Visual Studio 2017 15.8.7 on Windows 8.1 and 10.
Tested configurations are `Debug/Release;Console/Tiles;Win32/x64`, Python and
Lua support for editing are untested, and a webtiles build is not available.

1. To get the DCSS source, follow the steps in the [Getting The
   Source](#getting-the-source) section above.

    ```sh
    git clone https://github.com/crawl/crawl.git
    git submodule update --init
    ```

2. Install a perl environment, [Perl provides links to several Windows
   binaries](http://www.perl.org/get.html).

3. In the DCSS source, run `gen-all.cmd` inside `crawl-ref/source/util/`. This
   step must be executed any time you update to a new version of the source (or
   if you have modified tile data or unrandarts).

4. The first time you compile, you need to build the `Contribs` solution. This
   compiles various libraries which DCSS itself needs to build. This
   needs to be performed the first time you build, when changing to the `Debug`
   configuration, and when the contribs are
   updated. To do this open and compile `Contribs.sln` in
   `crawl-ref/source/contrib`. Make sure to set `Release` or `Debug` (not the
   library versions), as well as the desired architecture (Win32 or x64). Then
   build (or re-build) the solution.

5. Open `crawl-ref.sln` in Visual Studio, this is in `crawl-ref/source/MSVC/`.

6. Select `Debug` or `Release` from the build configurations menu on the main
   toolbar; `crawl.exe` is compiled by selecting "Build Solution" in the BUILD
   menu.

7. To build the `Debug` version:

   1. First build `Release` following the instructions above
   2. Set the `Debug` configuration
   3. Re-build the `Contribs`
   4. Re-build with the `Debug` configuration set

   Building the `Debug` version without first building the `Release`
   configuration will result in an error on startup. The `Release` build can
   still be debugged using the Visual Studio debugger.

### Maintenance notes:

MSVC solution files are finicky. Opening the "All Configurations" or
"All Platforms" will automatically change settings. This is typically unwanted.

Troubleshooting tips:

- Make sure Windows Universal C Runtime is installed in MSVC.
- Build Release before rebuilding both solutions with Debug
- Use "Rebuild Solution" to make sure all files are rewritten
- Make sure all projects use `/MD` (or `/MDd` for the debug version)
- Make sure the appropriate (`/MD` or `/MDd`) CRT libraries are included for
  SDL, crawl, and tilegen
  (https://msdn.microsoft.com/en-us/library/abx4dbyh.aspx)
- Make sure `libpng.dll`, `SDL2.dll`, and `SDL2_image.dll` are in
  `crawl-ref/source` after building the `Contribs` solution. These are copied
  post-build from their original location in
  `source/contrib/bin/8.0/$(Platform)`.
- Make sure `freetype.lib`, `libpng.lib`, `lua.lib`, `pcre.lib`, `SDL2.lib`,
  `SDL2_image.lib`, `SDL2main.lib`, `sqlite.lib`, and `zlib.lib` are in
  `source/contrib/bin/8.0/$(Platform)` after building the `Contribs` solution.
- Make sure `crawl.exe` and `tilegen.exe` are in `crawl-ref/source` after
  building the `crawl-ref` solution.
- `tilegen.exe` runs early during the `crawl.exe` build process, using
  `libpng.dll` to build PNG files inside `source/rtiles`. Breaking tilegen
  (e.g. by building a different `Contribs` configuration) after building these
  png files correctly will result in `tilegen.exe` crashing during the crawl
  build process, but will not stop the build from working.  `fresh.bat` inside
  the MSVC folder will clear these files, making sure `tilegen.exe` stops the
  build process if it fails.

## Advanced

DCSS looks for several data files when starting up. They include:

* Special level and vault layout (`dat/*.des`) files.
* Core Lua code (`dat/dlua/*.lua`).
* Descriptions for monsters and game features (`dat/descript/*.txt`).
* Definitions for monster dialogue and randart names (`dat/database/*.txt`).

All these files are in the source tree under `source/dat`.

DCSS will also look for documentation files when players invoke the help
system. These files are available under the docs directory.

Your built DCSS binary must be able to find these files, or it will not start.

If DCSS is built without an explicit `DATA_DIR_PATH` (this is the most common
setup), it will search for its data files under the current directory, and if
it can't find them there, one level above the current directory. In short, it
uses these search paths: `.`, `./dat`, `./docs`, `..`, `../dat`, `../docs`.

### ccache

If you're recompiling DCSS many times (eg while developing features), `ccache`
can significantly speed up this process. It is generally easy to install and
set up.

On Linux, try installing the `ccache` with your package manager. You will need
to add the ccache directory to your `PATH`.

On macOS, run `brew install ccache` and follow the instructions to update your
`PATH`.

### Installing For All Users

You might want to install DCSS if you want to allow all users on your machine
to play it.

Use `make install prefix=/usr/local` to build and install DCSS.

Make options:

* `prefix`: Specify the prefix to install to. You probably want `/usr` or
  `/usr/local`
* `SAVEDIR`: defaults to `~/.crawl`
* `DATADIR`: defaults to `$prefix/share/crawl`

### Desktop files and AppStream metadata

On Linux distributions and any other OS that follows the
[XDG specifications](https://www.freedesktop.org), you can install some
additional files to provide the required information for DCSS to be included in
applications menus and software centers. This can be particularly useful if you
are building DCSS to be distributed as a package.

Use `make install-xdg-data` to install the following files:

* A desktop file in `$prefix/share/applications`
* A metainfo file in `$prefix/share/metainfo`
* Several icons of different sizes in `$prefix/share/icons/hicolor`

The name of the files is governed by the `GAME` option in order to match
the generated executable file.

### .des level compiler

DCSS uses a level compiler to read the level design (.des) files in the
`source/dat` directory.

If you're using one of standard makefile, the steps described in this section
are performed automatically:

The level compiler source is in the `source/util` directory (`levcomp.lpp` and
`levcomp.ypp`). The steps involved in building the level compiler are:

* Run `flex` on `levcomp.lpp` to produce the `levcomp.lex.cc` lexer.
* Run `bison` on `levcomp.ypp` to produce the `levcomp.tab.cc` parser and
  `levcomp.tab.h`
* Compile the resulting C++ source files and `levcomp.cc` and link the object
  files into the DCSS executable.

For convenience on systems that don't have flex/bison, pre-generated
intermediate files are provided under `source/prebuilt`. The makefiles provided
with the DCSS source distribution will use these pre-generated files
automatically if flex/bison is not available.

### Code Coverage

Code coverage requires some more package to be installed. See
[testing.md](crawl-ref/docs/develop/testing.md) for more info.

### Lua

The Lua source is included with DCSS. It will be used if you don't have Lua
headers installed. Note that we don't provide security support for Lua, and
thus if you run a public server or a kiosk, it is strongly recommended to use
system Lua which does receive security updates from whatever distribution you
use.

### PCRE

PCRE 8.12, with a custom build system but otherwise unchanged, is included with
DCSS. It is enabled by default on Windows; otherwise, unless you build with
`BUILD_PCRE=y` (to use the contrib) or `USE_PCRE=y` (to use a development
package from your manager), the system POSIX regex will be used.

### Unicode

On Unix, you want an UTF-8 locale. All modern distributions install one by
default, but you might have somehow dropped required settings. To check this,
run `locale charmap`, which should say `UTF-8`. If it's not, please ensure
either `LANG`, `LC_ALL` or `LC_CTYPE` is set. A typical line would be `export
LC_ALL=en_US.UTF-8`.

On Windows, the console behaves differently for TrueType and legacy (bitmap)
fonts. The latter (mostly) work only on certain language editions of Windows,
such as English, and even there, they work adequately only for DCSS's default
settings. For anything more, please select one of TrueType fonts. If, like one
of our players, you are deeply attached to the looks of bitmap fonts, you can
[download a corrected version of the Terminal
font](http://www.yohng.com/software/terminalvector.html)

## Getting Help

The best place to ask for help is `#crawl-dev` on Libera IRC, where developers
chat.

You can also try [any of the community forums detailed in the
README](../README.md#community).
