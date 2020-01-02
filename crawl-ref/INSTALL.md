# Install instructions for Dungeon Crawl Stone Soup (DCSS)

## Contents

* [Installing Prebuilt Versions](#installing-prebuilt-versions)
* [Compiling](#compiling)
  * [Ubuntu / Debian](#ubuntu--debian)
  * [Fedora](#fedora)
  * [Void Linux](#void)
  * [Other Linux / Unix](#other-linux--unix)
  * [macOS](#macOS)
  * [Windows](#windows)
    * [MSYS2 (Recommended)](#msys2-recommended)
    * [Windows Subsystem for Linux (WSL)](#windows-subsystem-for-linux-wsl)
    * [Visual Studio](#visual-studio)
* [Advanced](#advanced)
  * [ccache](#ccache)
  * [Installing For All Users](#installing-for-all-users)
  * [.des Level Compiler](#des-level-compiler)
  * [Code Coverage](#code-coverage)
  * [Lua](#lua)
  * [PCRE](#pcre)
  * [Unicode](#unicode)

## Installing Prebuilt Versions

You can download prebuild versions of DCSS from the [DCSS homepage](https://crawl.develz.org/download.htm).

The rest of this file deals with compiling DCSS from source.

## Compiling

Here's the basic workflow. Please read the section for your OS below for more detailed instructions.

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
```

### Ubuntu / Debian

You can install all needed dependencies from the OS:

```sh
sudo apt install build-essential libncursesw5-dev bison flex liblua5.1-0-dev libsqlite3-dev libz-dev pkg-config python-yaml binutils-gold

# Additional dependencies for tiles builds
sudo apt install libsdl2-image-dev libsdl2-mixer-dev libsdl2-dev libfreetype6-dev libpng-dev ttf-dejavu-core
```

Then follow [the above compilation steps](#compiling).

### Fedora

These instructions will possibly also work for other RPM-based distros.

You can install all needed dependencies from the OS:

```sh
sudo dnf install gcc gcc-c++ make bison flex ncurses-devel compat-lua-devel sqlite-devel zlib-devel pkgconfig python-yaml

# Additional dependencies for tiles builds:
sudo dnf install SDL2-devel SDL2_image-devel libpng-devel freetype-devel dejavu-sans-fonts dejavu-sans-mono-fonts
```

Then follow [the above compilation steps](#compiling).

### Void

You can install all needed dependencies from the OS:

```sh
sudo xbps-install make gcc perl flex bison pkg-config ncurses-devel lua51-devel sqlite-devel zlib-devel python-yaml

# Additional dependencies for tiles builds:
sudo xbps-install pngcrush dejavu-fonts-ttf SDL2-devel SDL2_mixer-devel SDL2_image-devel freetype-devel
```

Then follow [the above compilation steps](#compiling).

## Other Linux / Unix

You need the following dependencies:

* GNU make
* gcc / clang
* perl
* pkg-config
* Python and PyYAML
* libncurses (and headers)
* flex / bison (optional)

These dependencies can be supplied by your OS, or installed from DCSS's submodules (`git submodule update --init`):

* lua 5.1 (and headers)
* sqlite (and headers)
* zlib (and headers)
* freetype (and headers)
* DejaVu fonts (when compiling in tiles mode)
* SDL2 (and headers)
* libpng (and headers)
* pcre (and headers)
* zlib (and headers)

Then follow [the above compilation steps](#compiling).

## macOS

1. Before building on macOS, you need a working copy of Xcode and the associated command line tools.
    1. Install Xcode from the App Store
    2. Open Xcode and say yes if you are prompted to install optional developer tools. (You can quit Xcode after this completes.)
    3. Run `xcode-select --install` in a Terminal

2. You will also need to install DCSS's bundled dependencies:

    ```sh
    cd crawl
    git submodule update --init
    ```

Then follow [the above compilation steps](#compiling).

## Windows

### MSYS2 (Recommended)

This is the only currently recommended process for building Crawl on windows.
It is also possible to cross-compile windows builds; see the release guide
for instructions.

MSYS2 is a Cygwin-derived software distro for Windows that can build
native applications; you will interact with the build process in a unix-like
shell interface. You can download the MSYS2 installer from the following
website:

https://msys2.github.io/

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
development packages and compile Crawl. The commands shown below should be run
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

3. At this point on current MSYS2 versions, your development environment should
  be complete. You can test it by running:

    ```sh
    gcc -v
    ```

    If this works, you're all set. If it doesn't, you may be an an older version
    of MSYS2 and need to manually add the newly installed toolchain to your
    path. To do so, run the following line, either at the command line (for
    that shell instance only) or in the file `~/.bashrc` to make it permanent:

    ```sh
    export PATH=$PATH:/mingw64/bin
    ```

4. To install PyYAML, you can install it from either pacman or Pip/PyPA:

    ```sh
    pacman -S mingw-w64-x86_64-python3-yaml
    # or
    pacman -S mingw64/mingw-w64-x86_64-python3-pip
    pip install pyyaml
    ```

    You can verify PyYAML is installed by running `python -m yaml`, which should
    give an error like `'yaml' is a package and cannot be directly imported`
    (rather than `No module named yaml`).

5. To get the Crawl source, follow the steps in the [Getting The Source](#getting-the-source)
  section above to clone Crawl into your MSYS2 home directory. We recommend
  using the MSYS2-installed version of git for these steps. In brief:

    1. `cd` to the location where you would like the crawl repository to be. It
      will clone into a folder named `crawl`. Your home directory (`cd ~`) is
      a reasonable choice.
    2. Run `git clone https://github.com/crawl/crawl.git`.
    3. Run `cd crawl/crawl-ref/source`.
    4. Run `git submodule update --init`.

6. Build the console version of Crawl by simply running:

    ```sh
    # console build
    make
    # tiles build
    make TILES=y
    ```

    If you want a debug build, add the target `debug` to the above commands (eg `make debug TILES=y`)
    For building packages, see instructions in the release guide.

7. When the build process finishes, you can run crawl.exe directly from the
  source directory in the MSYS2 shell. For Tiles, type `./crawl.exe`, and for
  console, type `start crawl`, which will open Crawl in a new command.exe
  window (the Windows version of Crawl requires a command.exe shell and will
  not run in an MSYS2 shell). Both versions can also be started by
  double-clicking crawl.exe using the graphical file explorer.

### Windows Subsystem for Linux (WSL)

These instructions have been tested with Ubuntu.

1. [Follow the instructions on Microsoft's website](https://docs.microsoft.com/en-us/windows/wsl/install-win10) at to set up the Windows Subsystem for Linux.

2. Follow the [Ubuntu instructions](#ubuntu--debian) to build Crawl.

3. Console and Webtiles will work without any further configuration. To get SDL
  tiles to work, you need an X server running on Windows; Xming is an option
  that has worked in the past. If you run into an error about not finding a
  matching GLX visual, try running:

    ```sh
    export SDL_VIDEO_X11_VISUALID=
    ```

### Visual Studio

This build process is currently unsupported, and unlikely
  to be straightforward in versions of MSVC besides those explicitly mentioned
  here.

1. You will need to download crawl, as well as its submodules. The easiest way to do
  this is with the commands

    ```sh
    git clone https://github.com/crawl/crawl.git
    git submodule update --init
    ```

2. This build is tested on Visual Studio 2017 15.8.7 on Windows 8.1 and 10.
  Supported configurations are Debug/Release;Console/Tiles;Win32/x64

3. You'll need a perl environment, there are links to several Windows binaries
  you can choose from at http://www.perl.org/get.html

4. In the Crawl source, run gen-all.cmd inside crawl-ref/source/util/. This step must
  be executed any time you update to a new version of the source (or if you
  have modified tile data or unrandarts).

5. To build the Debug version, first build Release, then re-build with the Debug
  configuration set. You will need to re-build the Contribs solution as well.
  Building the Debug version without first building the Release configuration will
  result in an error on startup.

6. The first time you compile, you need to build the Contribs solution. This
  compiles various libraries which Crawl itself needs to build. This only needs
  to be performed the first time you build, or if the contribs are ever updated
  (extremely rare). To do this open and compile Contribs.sln in crawl-ref/source/contrib.
  Make sure to set "Release" or "Debug" (not the library versions), as well as
  the desired architecture (Win32 or x64). Then build (or re-build) the solution.

7. Open crawl-ref.sln in Visual Studio, this is in crawl-ref/source/MSVC/.

8. You can now select Debug or Release from the drop-down build configurations
  menu on the main toolbar (it should be next to "Local Windows Debugger");
  crawl.exe can them be compiled by selecting "Build Solution" in the BUILD
  menu. You can quickly run it by selecting "Start without Debugging" in the
  debug menu (or debug with "Start Debugging"). Note: the Release build can
  still be debugged using the Visual Studio debugger.

9. Building the webserver and Lua (for the dat project) is untested as of now.

10. Better support for Python (for the webserver project) and Lua (for the dat
  project) can be installed with these extensions:

  Python Tools for Visual Studio
  http://pytools.codeplex.com/releases

  Ports of VsLua to Visual Studio 2012
  http://www.dbfinteractive.com/forum/index.php?topic=5873.0
  http://pouet.net/topic.php?which=9087

  Try at your own risk, but the first one has been tested and found to be
  effective.

### Maintenance notes:

MSVC solution files are finicky. Opening the "All Configurations" or
"All Platforms" will automatically change settings. This is typically unwanted.

Troubleshooting tips:

Make sure Windows Universal C Runtime is installed in MSVC.

Build Release before rebuilding both solutions with Debug

Use "Rebuild Solution" to make sure all files are rewritten

Make sure all projects use /MD (or /MDd for the debug version)

Make sure the appropriate (/MD or /MDd) CRT libraries are included for SDL,
crawl, and tilegen (https://msdn.microsoft.com/en-us/library/abx4dbyh.aspx)

Make sure libpng.dll, SDL2.dll, and SDL2_image.dll are in crawl-ref/source
after building the Contribs solution. These are copied post-build from their
original location in source/contrib/bin/8.0/$(Platform).

Make sure freetype.lib, libpng.lib, lua.lib, pcre.lib, SDL2.lib, SDL2_image.lib,
SDL2main.lib, sqlite.lib, and zlib.lib are in source/contrib/bin/8.0/$(Platform)
after building the Contribs solution.

Make sure crawl.exe and tilegen.exe are in crawl-ref/source after building the
crawl-ref solution.

Tilegen runs early during the crawl.exe build process, using libpng.dll to build PNG files
inside source/rtiles. Breaking tilegen (e.g. by building a different Contribs configuration)
after building these png files correctly will result in tilegen crashing during the crawl build
process, but will not stop the build from working (until it breaks for some other reason). fresh.bat
inside the MSVC folder will clear these files, making sure tilegen stops the build process if it fails.

tilegen depends on SDL2, SDL2_image, and libpng.
crawl depends on SDL2, SDL2_image, libpng, lua, pcre, sqlite, zlib, and the CRT libraries.
freetype uses the zlib source directory as an include.
libpng depends on zlib, and uses the zlib source directory as an include.
SDL2_image depends on SDL2 and SDL2main.
SDL2 depends on winmm.lib,imm32.lib,version.lib, and the CRT libraries.


## Advanced

Crawl looks for several data files when starting up. They include:

* Special level and vault layout (dat/*.des) files.
* Core Lua code (dat/dlua/*.lua).
* Descriptions for monsters and game features (dat/descript/*.txt).
* Definitions for monster dialogue and randart names (dat/database/*.txt).

All these files are in the source tree under source/dat.

Crawl will also look for documentation files when players invoke the help
system. These files are available under the docs directory.

Your built Crawl binary must be able to find these files, or it will not start.

If Crawl is built without an explicit DATA_DIR_PATH (this is the most common
setup), it will search for its data files under the current directory, and if
it can't find them there, one level above the current directory. In short, it
uses these search paths: ., ./dat, ./docs, .., ../dat, ../docs.

### ccache

If you're recompiling DCSS many times (eg while developing features), `ccache` can significantly speed up your compile times. It is generally easy to install and set up.

On Linux, try installing the `ccache` with your package manager. You will need to add the ccache directory to your `PATH`.

On macOS, run `brew install ccache` and follow the instructions to update your `PATH`.

### Installing For All Users

You might want to install DCSS if you want to allow all users on your machine to play it, rather than just yourself.

Use `make install prefix=/usr/local` to build and install DCSS.

Make options:

* `prefix`: Specify the prefix to install to. You probably want `/usr` or `/usr/local`
* `SAVEDIR`: defaults to `~/.crawl`
* `DATADIR`: defaults to `$prefix/share/crawl`

### .des level compiler

Crawl uses a level compiler to read the level design (.des) files in the
source/dat directory.

If you're using one of standard makefile, the steps described in this section
are performed automatically:

The level compiler source is in the source/util directory (levcomp.lpp and
levcomp.ypp). The steps involved in building the level compiler are:

* Run flex on levcomp.lpp to produce the levcomp.lex.cc lexer.
* Run bison on levcomp.ypp to produce the levcomp.tab.cc parser and
  levcomp.tab.h
* Compile the resulting C++ source files and levcomp.cc and link the object
  files into the Crawl executable.

For convenience on systems that don't have flex/bison, pre-generated
intermediate files are provided under source/prebuilt. The makefiles provided
with the Crawl source distribution will use these pre-generated files
automatically if flex/bison is not available.

### Code Coverage

Code coverage requires some additional packages, see [testing.md](crawl-ref/docs/develop/testing.md) for more info.

### Lua

The Lua source is included with Crawl. It will be used if you don't have Lua
headers installed. Note that we don't provide security support for Lua, and
thus if you run a public server or a kiosk, it is strongly recommended to use
system Lua which does receive security updates from whatever distribution you
use.

### PCRE

PCRE 8.12, with a custom build system but otherwise unchanged, is included with
Crawl. It is enabled by default on Windows; otherwise, unless you build with
BUILD_PCRE (to use the contrib) or USE_PCRE (to use a development package from
your manager), the system POSIX regex will be used.

### Unicode

On Unix, you want an UTF-8 locale. All modern distributions install one by
default, but you might have somehow dropped required settings. To check this,
run "locale charmap", which should say "UTF-8". If it's not, please ensure
either LANG, LC_ALL or LC_CTYPE is set. A typical line would be "export
LC_ALL=en_US.UTF-8".

On Windows, the console behaves differently for TrueType and legacy (bitmap)
fonts. The latter (mostly) work only on certain language editions of Windows,
such as English, and even there, they work adequately only for Crawl's default
settings. For anything more, please select one of TrueType fonts. If, like one
of our players, you are deeply attached to the looks of bitmap fonts, you can
download a corrected version of the Terminal font from
http://www.yohng.com/software/terminalvector.html
