cmake_minimum_required(VERSION 3.12)

################################################################################
# Prevent in source builds
################################################################################
get_filename_component(srcdir "${CMAKE_SOURCE_DIR}" REALPATH)
get_filename_component(bindir "${CMAKE_BINARY_DIR}" REALPATH)


if("${srcdir}" STREQUAL "${bindir}")
    message("################################################################################")
    message("# Crawl should not be configured & built in its source directory")
    message("# You must run cmake in a build directory.")
    message("# For example:")
    message("# mkdir crawl-build")
    message("# Then you can proceed to configure and build")
    message("# by using the following commands:")
    message("#")
    message("# cd crawl-build")
    message("# cmake <path to crawl-ref> -DTILE_MODE=LOCAL_TILES")
    message("# make")
    message("#")
    message("# NOTE: Given that you already tried to make an in-source build")
    message("#       CMake have already created several files & directories")
    message("#       in your source tree. run 'git status' to find them and")
    message("#       remove them by doing:")
    message("#")
    message("#       git clean -n -d")
    message("#       git clean -f -d")
    message("#       git checkout --")
    message("#")
    message("################################################################################")
    message(FATAL_ERROR "Quitting configuration")
endif()

################################################################################
# DCSS Project
################################################################################
project(dcss VERSION 0.23.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Will be filled in configuration section
set(COMMON_DEFINES "CLUA_BINDINGS")

include(ExternalProject)
include(FindPkgConfig)


################################################################################
# Project configuration
################################################################################
# The following main build configurations are supported:
# Linux native - crawl console
#       TILE_MODE=NO_TILES
#
# Linux native - crawl local tiles
#   TILE_MODE=LOCAL_TILES
#
# Linux native - crawl web tiles
#   TILE_MODE=WEB_TILES
#
# MacOS native - crawl console
#       TILE_MODE=NO_TILES
#
# MacOS native - crawl local tiles
#       TILE_MODE=LOCAL_TILES
#
# Currently unsupported:
# Linux to Windows cross compilation - crawl console
#   CROSSCOMPILE=ON
#   TILE_MODE=NO_TILES
#
# Linux to Windows cross compilation - crawl local tiles
#       CROSSCOMPILE=ON
#       TILE_MODE=LOCAL_TILES
#
# The following might be supported in the future:
# Windows native - crawl console TODO
# Windows native - crawl local tiles TODO

# Setting: USE_PREBUILT_LEVCOMP
# Can be 'ON' or 'OFF'. If this setting is OFF the prebuild header / source
# files are taken. Recomended is leave it 'OFF' (default) unless you absolutely
# do not have bison / flex available.
set(CACHE USE_PREBUILT_LEVCOMP ON BOOL
    "If USE_PREBUILT_LEVCOMP is set to OFF BISON and YACC are requird to
    generate the level compiler")

# Setting: TILE_MODE
# This controls for which display mode crawl will be compiled, i.e. console,
# local ui, web ui.
# Can be exactly one of : 'NO_TILES' | 'LOCAL_TILES' | 'WEB_TILES'
# Defaults to 'NO_TILES'
set(tile_build_modes NO_TILES LOCAL_TILES WEB_TILES)
set(TILE_MODE NO_TILES CACHE STRING
  "Defines if build with tiles, web tiles or no tiles, valid values are:
  NO_TILES, LOCAL_TILES, WEB_TILES")
set_property(CACHE TILE_MODE PROPERTY STRINGS ${tile_build_modes})
if(NOT TILE_MODE IN_LIST tile_build_modes)
    message(FATAL_ERROR "TILE_MODE must be one of ${tile_build_modes}")
endif()

if(TILE_MODE STREQUAL "NO_TILES")
    # do nothing
elseif(TILE_MODE STREQUAL "LOCAL_TILES")
    list(APPEND COMMON_DEFINES USE_TILE USE_TILE_LOCAL USE_SDL USE_GL USE_FT)
elseif(TILE_MODE STREQUAL "WEB_TILES")
    list(APPEND COMMON_DEFINES USE_TILE USE_TILE_WEB DGAMELAUNCH)
endif()

# Setting: USE_MERGE_BASE
# Use this to allow server maintainers to create version numbers identical with
# source version if they maintain extra changes to e.g. the build system / dist.
# on top of the current release.
# Set this to a valid commit id.
set(USE_MERGE_BASE "" CACHE STRING "")
set(MERGE_BASE "")
if(NOT USE_MERGE_BASE STREQUAL "")
    execute_process(
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMAND git merge-base HEAD ${USE_MERGE_BASE}
        OUTPUT_VARIABLE MERGE_BASE
        ERROR_VARIABLE MERGE_BASE_ERROR
        RESULT_VARIABLE MERGE_BASE_RETURN_CODE
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(MERGE_BASE_RETURN_CODE)
        message(FATAL_ERROR "Cannot find merge-base: ${MERGE_BASE_ERROR}")
    endif()
endif()

# Setting ADDITIONAL_DEFINES
# Here you can provide a list of defines to pass on to the build. Note that
# cmake expects a semicolon seperated list of defines. E.g. call
# '-DADDITIONAL_DEFINES="CLUA_BINDINGS;DGAMELAUNCH"' from the comand line to
# pass the CLUA_BINDINGS and DGAMELAUNCH define to the compilation.
# Be aware that the defines are passed on to each an every compilation in this
# build.
set(CACHE ADDITIONAL_DEFINES "" CACHE STRING
    "Additional preprocessor defines to pass on to the build ca be added here.
    To for example pass two defines to the build set ADDITIONAL_DEFINES to:
    \"FOO;BAR=1\"")
list(APPEND COMMON_DEFINES ${ADDITIONAL_DEFINES})


################################################################################
# Build machine introspection
################################################################################

# Host identification
execute_process(
    COMMAND cc -dumpmachine
    OUTPUT_VARIABLE DUMPMACHINE
    ERROR_VARIABLE DUMPMACHINE_ERROR
    RESULT_VARIABLE DUMPMACHINE_RETURN_CODE
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(DUMPMACHINE_RETURN_CODE)
    message(WARNING
        "Cannot identify host. Call to 'cc -dumpmachine' failed with: ${DUMPMACHINE_ERROR}")
    set(HOST "unknown")
else()
    set(HOST ${DUMPMACHINE})
endif()
message(STATUS "Host identification is: ${HOST}")

# Target architecture
# Cross builds are not yet supported
set(ARCH ${HOST})
message(STATUS "Target acitecure is: ${ARCH}")


################################################################################
# Deps
################################################################################
# A note about external dependencies in contrib: Native builds for Linux and
# MacOS should use OS packages. Dependencies in contrib are only used for cross
# compilation builds Linux to Windows.

# Lua
find_package(Lua 5.1 EXACT REQUIRED)
#ExternalProject_Add(build-lua
#    BUILD_IN_SOURCE ON
#    SOURCE_DIR "${CMAKE_SOURCE_DIR}/source/contrib/lua/src"
#    CONFIGURE_COMMAND ""
#    BUILD_COMMAND make install prefix=${CMAKE_BINARY_DIR}/contrib/lua
#    INSTALL_COMMAND ""
#)
#file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/contrib/lua/include)
#add_library(lua STATIC IMPORTED GLOBAL)
#set_target_properties(lua PROPERTIES
#    IMPORTED_LOCATION "${CMAKE_BINARY_DIR}/contrib/lua/lib/liblua.a"
#    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_BINARY_DIR}/contrib/lua/include"
#)
#add_dependencies(lua build-lua)

# Zlib
find_package(ZLIB REQUIRED)
#ExternalProject_Add(build-zlib
#    SOURCE_DIR "${CMAKE_SOURCE_DIR}/source/contrib/zlib"
#    CMAKE_ARGS
#        -Wno-dev
#        "-DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/contrib/zlib"
#)
#file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/contrib/zlib/include)
#add_library(zlib STATIC IMPORTED GLOBAL)
#set_target_properties(zlib PROPERTIES
#    IMPORTED_LOCATION "${CMAKE_BINARY_DIR}/contrib/zlib/lib/libz.a"
#    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_BINARY_DIR}/contrib/zlib/include"
#)
#add_dependencies(zlib build-zlib)

# Perl
find_package(Perl REQUIRED)

# Sqlite
pkg_search_module(sqlite REQUIRED sqlite3 IMPORTED_TARGET GLOBAL)

#file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/contrib/sqlite/include)
#ExternalProject_Add(build-sqlite
#    BUILD_IN_SOURCE ON
#    SOURCE_DIR "${CMAKE_SOURCE_DIR}/source/contrib/sqlite"
#    CONFIGURE_COMMAND ""
#    BUILD_COMMAND make install prefix=${CMAKE_BINARY_DIR}/contrib/sqlite
#    INSTALL_COMMAND ""
#)
#add_library(sqlite STATIC IMPORTED GLOBAL)
#set_target_properties(sqlite PROPERTIES
#    IMPORTED_LOCATION "${CMAKE_BINARY_DIR}/contrib/sqlite/lib/libsqlite3.a"
#    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_BINARY_DIR}/contrib/sqlite/include"
#)
#add_dependencies(sqlite build-sqlite)

# Frontend (console, local tiles, web tiles)
if(TILE_MODE STREQUAL NO_TILES OR TILE_MODE STREQUAL WEB_TILES)
    if(APPLE)
        find_package(Curses REQUIRED)
        add_library(curses SHARED IMPORTED GLOBAL)
        set_target_properties(curses PROPERTIES
          IMPORTED_LOCATION ${CURSES_LIBRARY}
            INTERFACE_INCLUDE_DIRECTORIES ${CURSES_INCLUDE_DIRS}
        )
    else()
        pkg_search_module(curses REQUIRED ncursesw IMPORTED_TARGET GLOBAL)
        add_library(curses ALIAS PkgConfig::curses)
    endif()
endif()

if(TILE_MODE STREQUAL LOCAL_TILES OR TILE_MODE STREQUAL WEB_TILES)
    # Fonts
    set(FONT_SEARCH_LOCATIONS
        /usr/share/fonts
        /usr/local/share/fonts
        /usr/*/lib/X11/fonts
    )

    set(OUR_PROPORTIONAL_FONT DejaVuSans.ttf)
    find_file(PROPORTIONAL_FONT
        ${OUR_PROPORTIONAL_FONT}
        PATHS ${FONT_SEARCH_LOCATIONS}
        NO_DEFAULT_PATH
    )
    if(NOT PROPORTIONAL_FONT)
        message(STATUS "Proportional font could not be found on system. Using our font.")
        set(PROPORTIONAL_FONT ${CMAKE_SOURCE_DIR}/source/contrib/fonts/${OUR_PROPORTIONAL_FONT})
    endif()

    set(OUR_MONOSPACED_FONT DejaVuSansMono.ttf)
    find_file(MONOSPACED_FONT
        ${OUR_MONOSPACED_FONT}
        PATHS ${FONT_SEARCH_LOCATIONS}
        NO_DEFAULT_PATH
    )
    if(NOT MONOSPACED_FONT)
        message(STATUS "Monospace font could not be found on system. Using our font.")
        set(MONOSPACED_FONT ${CMAKE_SOURCE_DIR}/source/contrib/fonts/${OUR_MONOSPACED_FONT})
    endif()

    set(COMMON_DEFINES
        ${COMMON_DEFINES}
        PROPORTIONAL_FONT="${PROPORTIONAL_FONT}"
        MONOSPACED_FONT="${MONOSPACED_FONT}"
    )

    find_package(PNG REQUIRED)
endif()

if(TILE_MODE STREQUAL LOCAL_TILES)
    # SDL2
    pkg_search_module(SDL2 REQUIRED sdl2 IMPORTED_TARGET GLOBAL)

    # SDL_Image 2
    pkg_search_module(SDL2_IMAGE REQUIRED SDL2_image>=2 IMPORTED_TARGET GLOBAL)

    # OpenGL
    find_package(OpenGL REQUIRED)

    #Freetype2
    find_package(Freetype REQUIRED)
endif()

# Threads
find_package(Threads REQUIRED)

if(NOT USE_PREBUILT_LEVCOMP)
    # Bison (YACC)
    find_package(BISON REQUIRED)

    # Flex (LEX)
    find_package(FLEX REQUIRED)
endif()


################################################################################
# Crawl
################################################################################
add_subdirectory(source)
add_subdirectory(source/util)
add_subdirectory(source/rltiles)
add_subdirectory(source/rltiles/tool)
