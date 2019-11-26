# TODO use native host compiler when crosscompiling
# Do not crosscompile! This tool is used as a code generator during a build

set(target tilegen)

set(header_files
    tile_colour.h
    tile.h
    tile_list_processor.h
    tile_page.h
)

set(source_files
    main.cc
    tile.cc
    tile_colour.cc
    tile_list_processor.cc
    tile_page.cc
)

add_executable(${target}
    ${header_files}
    ${source_files}
)

target_compile_definitions(${target}
    PUBLIC
        ${COMMON_DEFINES}
)

target_link_libraries(${target}
    $<$<NOT:$<STREQUAL:${TILE_MODE},NO_TILES>>:PNG::PNG>
)
