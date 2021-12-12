# AppImage

## Contents

* [Introduction](#introduction)
* [Creating an AppImage](#creating-an-appimage)
* [How does it work?](#how-does-it-work)

## Introduction

AppImage is a format for distributing portable software on Linux without needing
superuser permissions to install the application. It tries also to allow Linux
distribution-agnostic binary software deployment for application developers,
also called upstream packaging.

AppImages are very simple to use, just two steps required:

1. Make it executable
2. Run

This can be done either with the command line or a GUI. It features some other
uses like mounting or desktop integration. You can browse the [AppImage
documentation](https://docs.appimage.org/) for more details.

## Creating an AppImage

When building Crawl for Linux targets, you can easily create an AppImage with
the help of the `linuxdeploy` tool.

1. [Download the linuxdeploy AppImage](
   https://github.com/linuxdeploy/linuxdeploy/releases)

2. Make it executable.

    ```sh
    chmod +x /path/to/linuxdeploy.AppImage
    ```

3. Follow the usual Linux compilation procedure and, when running `make`,
   include the `appimage` target and the path to `linuxdeploy` in the
   `LINUXDEPLOY` parameter.

    ```sh
    # console build
    make LINUXDEPLOY=/path/to/linuxdeploy.AppImage appimage
    # tiles build
    make TILES=y LINUXDEPLOY=/path/to/linuxdeploy.AppImage appimage
    ```

## How does it work?

Running

    ```sh
    make LINUXDEPLOY=/path/to/linuxdeploy.AppImage appimage
    ```

is roughly equivalent to

    ```sh
    make DESTDIR=appimage/ prefix=usr install install-xdg-data
    /path/to/linuxdeploy.AppImage --appdir appimage --output appimage
    ```

1. First, a normal Linux build is performed.

2. Crawl is installed under the `appimage/usr` directory, including the desktop
   files and AppStream metadata.

3. Then `linuxdeploy` reads the contents of the `appimage` directory and
   completes the basic AppDir structure.

4. The dependencies for the executable file are copied  to
   `appimage/usr/bin/lib`.

5. The AppDir root directory is completed with some resource files copied or
   symlinked.

6. The AppStream metadata is validated.

7. Finally, the `appimage` directory is packaged into a Squashfs filesystem.
   The resulting file will be named as:
   `stone_soup-<version>-linux-<tiles/console>.<arch>.AppImage`

