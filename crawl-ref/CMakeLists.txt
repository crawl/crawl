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

function(debug_print variable)
    message(STATUS "${variable}=${${variable}}")
endfunction()

# Det default build type to "Debug"
set(default_build_type "Debug")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
      STRING "Choose the type of build." FORCE)
endif()

# Setting: USE_PREBUILT_LEVCOMP
# Can be 'ON' or 'OFF'. If this setting is OFF the prebuild header / source
# files are taken. Recomended is leave it 'OFF' (default) unless you absolutely
# do not have bison / flex available.
set(USE_PREBUILT_LEVCOMP ON CACHE BOOL
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
# cmake expects a semicolon separated list of defines. E.g. call
# '-DADDITIONAL_DEFINES="CLUA_BINDINGS;DGAMELAUNCH"' from the command line to
# pass the CLUA_BINDINGS and DGAMELAUNCH define to the compilation.
# Be aware that the defines are passed on to each an every compilation in this
# build.
set(ADDITIONAL_DEFINES "" CACHE STRING
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
# Always required dependencies
################################################################################
# All of the following dependenies are required for all modes.
# A note about external dependencies in contrib: Native builds for Linux use OS
# packages. Dependencies in contrib are only used for MacOS compilation.
# Linux cross compilation and compilation on Windows are not yet supported.
if("${CMAKE_SYSTEM}" MATCHES "Linux")
    set(use_contrib_default OFF)
elseif("${CMAKE_SYSTEM}" MATCHES "Darwin")
    set(use_contrib_default ON)
else()
    set(use_contrib_default OFF)
endif()
set(USE_ALL_DEPS_FROM_CONTRIB ${use_contrib_default} CACHE BOOL
    "Controls if dependencies are taken from contrib or system")

################################################################################
# Lua
################################################################################
set(USE_LUA_FROM_CONTRIB OFF CACHE BOOL
    "Controls if Lua dependency is taken from contrib or system")

if(USE_ALL_DEPS_FROM_CONTRIB OR USE_LUA_FROM_CONTRIB)
    message(STATUS "Using Lua from contrib")
    ExternalProject_Add(build-lua
        BUILD_IN_SOURCE ON
        SOURCE_DIR "${CMAKE_SOURCE_DIR}/source/contrib/lua/src"
        CONFIGURE_COMMAND ""
        BUILD_COMMAND make prefix=${CMAKE_BINARY_DIR}/contrib
        INSTALL_COMMAND make install prefix=${CMAKE_BINARY_DIR}/contrib
        BUILD_BYPRODUCTS ${CMAKE_BINARY_DIR}/contrib/lib/liblua.a
    )
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/contrib/lua/include)
    add_library(lua STATIC IMPORTED GLOBAL)
    set_target_properties(lua PROPERTIES
        IMPORTED_LOCATION "${CMAKE_BINARY_DIR}/contrib/lib/liblua.a"
        INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_BINARY_DIR}/contrib/include"
    )
    add_dependencies(lua build-lua)
else()
    message(STATUS "Using Lua from system")
    find_package(Lua 5.1 EXACT REQUIRED)
    # Currently the FindLua module does not export a lua target, only old style
    # LUA_LIBRARIES and LUA_INCLUDE_DIR are exported. Hence we create a import
    # target ourselves
    add_library(lua INTERFACE)
    set_target_properties(lua PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${LUA_INCLUDE_DIR}"
    )
    target_link_libraries(lua INTERFACE ${LUA_LIBRARIES})
endif()

################################################################################
# Zlib
################################################################################
set(USE_ZLIB_FROM_CONTRIB OFF CACHE BOOL
    "Controls if zlib dependency is taken from contrib or system")

if(USE_ALL_DEPS_FROM_CONTRIB OR USE_ZLIB_FROM_CONTRIB)
    message(STATUS "Using zlib from contrib")
    ExternalProject_Add(build-zlib
        BUILD_IN_SOURCE ON
        SOURCE_DIR "${CMAKE_SOURCE_DIR}/source/contrib/zlib"
        CONFIGURE_COMMAND ""
        BUILD_COMMAND make prefix=${CMAKE_BINARY_DIR}/contrib
        INSTALL_COMMAND make install prefix=${CMAKE_BINARY_DIR}/contrib
        BUILD_BYPRODUCTS "${CMAKE_BINARY_DIR}/contrib/lib/libz.a"
    )
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/contrib/include)
    add_library(ZLIB::ZLIB STATIC IMPORTED GLOBAL)
    set_target_properties(ZLIB::ZLIB PROPERTIES
       IMPORTED_LOCATION "${CMAKE_BINARY_DIR}/contrib/lib/libz.a"
       INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_BINARY_DIR}/contrib/include"
    )
    add_dependencies(ZLIB::ZLIB build-zlib)
else()
    message(STATUS "Using zlib from system")
    find_package(ZLIB REQUIRED)
endif()

################################################################################
# Sqlite
################################################################################
set(USE_SQLITE_FROM_CONTRIB OFF CACHE BOOL
    "Controls if sqlite dependency is taken from contrib or system")
if(USE_ALL_DEPS_FROM_CONTRIB OR USE_SQLITE_FROM_CONTRIB)
    message(STATUS "Using sqlite from contrib")
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/contrib/include)
    ExternalProject_Add(build-sqlite
       BUILD_IN_SOURCE ON
       SOURCE_DIR "${CMAKE_SOURCE_DIR}/source/contrib/sqlite"
       CONFIGURE_COMMAND ""
       BUILD_COMMAND make prefix=${CMAKE_BINARY_DIR}/contrib
       INSTALL_COMMAND make install prefix=${CMAKE_BINARY_DIR}/contrib
       BUILD_BYPRODUCTS "${CMAKE_BINARY_DIR}/contrib/lib/libsqlite3.a"
    )
    add_library(sqlite STATIC IMPORTED GLOBAL)
    set_target_properties(sqlite PROPERTIES
       IMPORTED_LOCATION "${CMAKE_BINARY_DIR}/contrib/lib/libsqlite3.a"
       INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_BINARY_DIR}/contrib/include"
    )
    add_dependencies(sqlite build-sqlite)
else()
    message(STATUS "Using sqlite from system")
    pkg_search_module(sqlite REQUIRED sqlite3 IMPORTED_TARGET GLOBAL)
    add_library(sqlite ALIAS PkgConfig::sqlite)
endif()

################################################################################
# Perl
################################################################################
find_package(Perl REQUIRED)

################################################################################
# Threads
################################################################################
find_package(Threads REQUIRED)

################################################################################
# Conditional dependencies
################################################################################
if(NOT USE_PREBUILT_LEVCOMP)
    find_package(BISON REQUIRED)
    find_package(FLEX REQUIRED)
endif()

set(NEEDS_CURSES OFF)
set(NEEDS_PNG OFF)
set(NEEDS_SDL2 OFF)
set(NEEDS_SDL2_IMAGE OFF)
set(NEEDS_OPENGL OFF)
set(NEEDS_FREETYPE2 OFF)

if(TILE_MODE STREQUAL LOCAL_TILES)
    set(NEEDS_PNG ON)
    set(NEEDS_SDL2 ON)
    set(NEEDS_SDL2_IMAGE ON)
    set(NEEDS_OPENGL ON)
    set(NEEDS_FREETYPE2 ON)
endif()

################################################################################
# libpng
################################################################################
set(USE_LIBPNG_FROM_CONTRIB OFF CACHE BOOL
    "Controls if libpng dependency is taken from contrib or system")
if(NEEDS_PNG)
    if(USE_ALL_DEPS_FROM_CONTRIB OR USE_LIBPNG_FROM_CONTRIB)
        message(STATUS "Using libpng from contrib")
        file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/contrib/include)
        ExternalProject_Add(build-png
            BUILD_IN_SOURCE ON
            SOURCE_DIR "${CMAKE_SOURCE_DIR}/source/contrib/libpng"
            CONFIGURE_COMMAND ""
            BUILD_COMMAND make
            INSTALL_COMMAND make install prefix=${CMAKE_BINARY_DIR}/contrib
            BUILD_BYPRODUCTS "${CMAKE_BINARY_DIR}/contrib/lib/libpng.a"
        )
        add_library(png STATIC IMPORTED GLOBAL)
        set_target_properties(png PROPERTIES
            IMPORTED_LOCATION "${CMAKE_BINARY_DIR}/contrib/lib/libpng.a"
            INTERFACE_INCLUDE_DIRECTORIES
                "${CMAKE_BINARY_DIR}/contrib/include"
        )
        add_dependencies(png build-png)
    else()
        message(STATUS "Using libpng from system")
        find_package(PNG REQUIRED)
    endif()
endif()

################################################################################
# SDL2
################################################################################
set(USE_SDL2_FROM_CONTRIB OFF CACHE BOOL
    "Controls if SDL2 dependency is taken from contrib or system")
if(NEEDS_SDL2)
    if(USE_ALL_DEPS_FROM_CONTRIB OR USE_SDL2_FROM_CONTRIB)
        message(STATUS "Using sdl2 from contrib")
        file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/contrib/include/SDL2)
        ExternalProject_Add(build-sdl2
            BUILD_IN_SOURCE ON
            SOURCE_DIR "${CMAKE_SOURCE_DIR}/source/contrib/sdl2"
            CONFIGURE_COMMAND ""
            BUILD_COMMAND make prefix=${CMAKE_BINARY_DIR}/contrib
            INSTALL_COMMAND make install prefix=${CMAKE_BINARY_DIR}/contrib
            BUILD_BYPRODUCTS "${CMAKE_BINARY_DIR}/contrib/lib/libSDL2.a"
        )
        add_library(sdl2 STATIC IMPORTED GLOBAL)
        set_target_properties(sdl2 PROPERTIES
            IMPORTED_LOCATION "${CMAKE_BINARY_DIR}/contrib/lib/libSDL2.a"
            INTERFACE_INCLUDE_DIRECTORIES
                "${CMAKE_BINARY_DIR}/contrib/include/SDL2"
        )
        add_dependencies(sdl2 build-sdl2)
        target_link_libraries(sdl2 INTERFACE dl png)
    else()
        message(STATUS "Using sdl2 from system")
        pkg_search_module(SDL2 REQUIRED sdl2 IMPORTED_TARGET GLOBAL)
        add_library(sdl2 ALIAS PkgConfig::SDL2)
    endif()
endif()

################################################################################
# SDL2 Image
################################################################################
set(USE_SDL2_IMAGE_FROM_CONTRIB OFF CACHE BOOL
    "Controls if SDL2-image dependency is taken from contrib or system")
if(NEEDS_SDL2_IMAGE)
    if(USE_ALL_DEPS_FROM_CONTRIB OR USE_SDL2_IMAGE_FROM_CONTRIB)
        message(STATUS "Using sdl2-image from contrib")
        file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/contrib/include)
        ExternalProject_Add(build-sdl2-image
            BUILD_IN_SOURCE ON
            SOURCE_DIR "${CMAKE_SOURCE_DIR}/source/contrib/sdl2-image"
            CONFIGURE_COMMAND ""
            BUILD_COMMAND make prefix=${CMAKE_BINARY_DIR}/contrib
            INSTALL_COMMAND make install prefix=${CMAKE_BINARY_DIR}/contrib
            BUILD_BYPRODUCTS
                "${CMAKE_BINARY_DIR}/contrib/lib/libSDL2_image.a"
            DEPENDS build-sdl2 build-png
        )
        add_library(sdl2-image STATIC IMPORTED GLOBAL)
        set_target_properties(sdl2-image PROPERTIES
            IMPORTED_LOCATION
                "${CMAKE_BINARY_DIR}/contrib/lib/libSDL2_image.a"
            INTERFACE_INCLUDE_DIRECTORIES
                "${CMAKE_BINARY_DIR}/contrib/include"
        )
        add_dependencies(sdl2-image build-sdl2-image build-sdl2 build-png)
        target_link_libraries(sdl2-image INTERFACE sdl2)
    else()
        message(STATUS "Using sdl2-image from system")
        pkg_search_module(SDL2_IMAGE REQUIRED SDL2_image>=2 IMPORTED_TARGET GLOBAL)
        add_library(sdl2-image ALIAS PkgConfig::SDL2_IMAGE)
    endif()
endif()

################################################################################
# OpenGL
################################################################################
if(NEEDS_OPENGL)
    find_package(OpenGL REQUIRED)
endif()

################################################################################
# Freetype2
################################################################################
set(USE_FREETYPE2_FROM_CONTRIB OFF CACHE BOOL
    "Controls if Freetype2 dependency is taken from contrib or system")
if(NEEDS_FREETYPE2)
    if(USE_ALL_DEPS_FROM_CONTRIB OR USE_FREETYPE2_FROM_CONTRIB)
        message(STATUS "Using Freetype2 from contrib")
        file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/contrib/include)
        ExternalProject_Add(build-freetype
            BUILD_IN_SOURCE ON
            SOURCE_DIR "${CMAKE_SOURCE_DIR}/source/contrib/freetype"
            CONFIGURE_COMMAND ""
            BUILD_COMMAND make prefix=${CMAKE_BINARY_DIR}/contrib
            INSTALL_COMMAND make install prefix=${CMAKE_BINARY_DIR}/contrib
            BUILD_BYPRODUCTS
                "${CMAKE_BINARY_DIR}/contrib/lib/libfreetype.a"
        )
        add_library(freetype STATIC IMPORTED GLOBAL)
        set_target_properties(freetype PROPERTIES
            IMPORTED_LOCATION
                "${CMAKE_BINARY_DIR}/contrib/lib/libfreetype.a"
            INTERFACE_INCLUDE_DIRECTORIES
                "${CMAKE_BINARY_DIR}/contrib/include/freetype2"
        )
        add_dependencies(freetype build-freetype)
    else()
        message(STATUS "Using Freetype2 from system")
        find_package(Freetype REQUIRED)
        add_library(freetype ALIAS Freetype::Freetype)
    endif()
endif()


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

################################################################################
# Crawl
################################################################################
add_subdirectory(source)
add_subdirectory(source/util)
add_subdirectory(source/rltiles)
add_subdirectory(source/rltiles/tool)
