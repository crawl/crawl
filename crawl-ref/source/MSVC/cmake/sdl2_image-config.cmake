# SDL2_image CMake configuration file:
# This file is meant to be placed in a cmake subfolder of SDL2_image-devel-2.x.y-VC

include(FeatureSummary)
set_package_properties(SDL2_image PROPERTIES
    URL "https://www.libsdl.org/projects/SDL_image/"
    DESCRIPTION "SDL_image is an image file loading library"
)

cmake_minimum_required(VERSION 3.0)

set(SDL2_image_FOUND TRUE)

set(SDL2IMAGE_AVIF  FALSE)
set(SDL2IMAGE_BMP   TRUE)
set(SDL2IMAGE_GIF   TRUE)
set(SDL2IMAGE_JPG   TRUE)
set(SDL2IMAGE_JXL   FALSE)
set(SDL2IMAGE_LBM   TRUE)
set(SDL2IMAGE_PCX   TRUE)
set(SDL2IMAGE_PNG   TRUE)
set(SDL2IMAGE_PNM   TRUE)
set(SDL2IMAGE_QOI   TRUE)
set(SDL2IMAGE_SVG   TRUE)
set(SDL2IMAGE_TGA   TRUE)
set(SDL2IMAGE_TIF   FALSE)
set(SDL2IMAGE_XCF   FALSE)
set(SDL2IMAGE_XPM   TRUE)
set(SDL2IMAGE_XV    TRUE)
set(SDL2IMAGE_WEBP  FALSE)

set(SDL2IMAGE_JPG_SAVE FALSE)
set(SDL2IMAGE_PNG_SAVE FALSE)

set(SDL2IMAGE_VENDORED  FALSE)

set(SDL2IMAGE_BACKEND_IMAGEIO   FALSE)
set(SDL2IMAGE_BACKEND_STB       TRUE)
set(SDL2IMAGE_BACKEND_WIC       FALSE)

if(CMAKE_SIZEOF_VOID_P STREQUAL "4")
    set(_sdl_arch_subdir "x86")
elseif(CMAKE_SIZEOF_VOID_P STREQUAL "8")
    set(_sdl_arch_subdir "x64")
else()
    unset(_sdl_arch_subdir)
    set(SDL2_image_FOUND FALSE)
    return()
endif()

set(_sdl2image_incdir       "${CMAKE_CURRENT_LIST_DIR}/../include")
set(_sdl2image_library      "${CMAKE_CURRENT_LIST_DIR}/../lib/${_sdl_arch_subdir}/SDL2_image.lib")
set(_sdl2image_dll          "${CMAKE_CURRENT_LIST_DIR}/../lib/${_sdl_arch_subdir}/SDL2_image.dll")

# All targets are created, even when some might not be requested though COMPONENTS.
# This is done for compatibility with CMake generated SDL2_image-target.cmake files.

if(NOT TARGET SDL2_image::SDL2_image)
    add_library(SDL2_image::SDL2_image SHARED IMPORTED)
    set_target_properties(SDL2_image::SDL2_image
        PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${_sdl2image_incdir}"
            IMPORTED_IMPLIB "${_sdl2image_library}"
            IMPORTED_LOCATION "${_sdl2image_dll}"
            COMPATIBLE_INTERFACE_BOOL "SDL2_SHARED"
            INTERFACE_SDL2_SHARED "ON"
    )
endif()

unset(_sdl_arch_subdir)
unset(_sdl2image_incdir)
unset(_sdl2image_library)
unset(_sdl2image_dll)
