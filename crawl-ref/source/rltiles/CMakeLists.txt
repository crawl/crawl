set(input_files
    dc-main.txt
    dc-dngn.txt
    dc-floor.txt
    dc-wall.txt
    dc-feat.txt
    dc-player.txt
    dc-gui.txt
    dc-icons.txt
)


################################################################################
# Setup stage folder (Some additional setup is done for WEB_TILES in the parrent
# CMakeLists.txt)
set(generated_files "")
set(generated_png_files "")
set(generated_png_files_target_location "")
foreach(input_file ${input_files})
    get_filename_component(input_file_stem_with_prefix ${input_file} NAME_WE)
    string(REGEX REPLACE "^dc-" "" input_file_stem
        ${input_file_stem_with_prefix})
    set(expected_output
        ${CMAKE_CURRENT_SOURCE_DIR}/tiledef-${input_file_stem}.h
        ${CMAKE_CURRENT_SOURCE_DIR}/tiledef-${input_file_stem}.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/tileinfo-${input_file_stem}.js
    )
    if(NOT TILE_MODE STREQUAL "NO_TILES")
        list(APPEND expected_output
            ${CMAKE_CURRENT_SOURCE_DIR}/${input_file_stem}.png)
        list(APPEND generated_png_files
            ${CMAKE_CURRENT_SOURCE_DIR}/${input_file_stem}.png)
        list(APPEND generated_png_files_target_location
            ${CMAKE_SOURCE_DIR}/source/dat/tiles/${input_file_stem}.png)
    endif()
    add_custom_command(
        OUTPUT ${expected_output}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMAND $<TARGET_FILE:tilegen> dc-${input_file_stem}.txt
        DEPENDS art-data ${input_files}
    )
    list(APPEND generated_files ${expected_output})
endforeach()

if(generated_png_files_target_location)
    add_custom_command(
        OUTPUT ${generated_png_files_target_location}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy ${generated_png_files}
                ${CMAKE_SOURCE_DIR}/source/dat/tiles
        DEPENDS ${generated_png_files}
    )
    add_custom_target(copy-generated-png-files-to-dat
        DEPENDS ${generated_png_files_target_location}
    )
else()
    # This is implemented as a NO-OP because CMake does not allow to add a
    # dependency from 'crawl' to this target here because at this point 'crawl'
    # is unknown. Sadly this is also not possible to be done with a generator
    # expression when defining 'crawl' dependencies. So to prevent an ugly-ass
    # if(NO_TILES) ... else() ... add_dependencies,
    # copy-generated-png-files-to-dat is simply a no-op when there is no need
    # for generated PNG files.
    add_custom_target(copy-generated-png-files-to-dat)
endif()

set(target rltiles)

set(header_files
    tiledef-unrand.h
    tiledef_defines.h
)

add_library(${target} STATIC
    ${header_files}
    ${generated_files}
)

target_compile_options(${target}
    PUBLIC
        $<$<CXX_COMPILER_ID:AppleClang>:-Wno-logical-op-parentheses>
)

target_include_directories(${target} PUBLIC
    .
    ..
)

target_link_libraries(${target} PUBLIC
    ZLIB::ZLIB
)
