################################################################################
# Generate files
################################################################################

### art-data.pl ################################################################
set(art_data_output_headers
    ${CMAKE_CURRENT_SOURCE_DIR}/art-data.h
    ${CMAKE_CURRENT_SOURCE_DIR}/art-enum.h
)
set(art_data_output
    ${CMAKE_CURRENT_SOURCE_DIR}/rltiles/dc-unrand.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/rltiles/tiledef-unrand.cc
)

add_custom_command(
    OUTPUT ${art_data_output} ${art_data_output_headers}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND util/art-data.pl
)

add_custom_target(art-data DEPENDS
    ${art_data_output}
    ${art_data_output_headers}
)

### gen-mst.pl #################################################################
set(gen_mst_output
    ${CMAKE_CURRENT_SOURCE_DIR}/mon-mst.h
)

add_custom_command(
    OUTPUT ${gen_mst_output}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND util/gen-mst.pl
)

add_custom_target(gen-mst DEPENDS ${gen_mst_output})

### cmd-name.pl #################################################################
set(cmd_name_output
    ${CMAKE_CURRENT_SOURCE_DIR}/cmd-name.h
)

add_custom_command(
    OUTPUT ${cmd_name_output}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND util/cmd-name.pl
)

add_custom_target(cmd-name DEPENDS ${cmd_name_output})

### gen-luatags.pl #############################################################
set(gen_luatags_output
    ${CMAKE_CURRENT_SOURCE_DIR}/dat/dlua/tags.lua
)

add_custom_command(
    OUTPUT ${gen_luatags_output}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND util/gen-luatags.pl
)

add_custom_target(copy-dat-to-stage
    COMMENT "Copy dat to stage"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/stage
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/source/dat dat
    DEPENDS ${gen_luatags_output}
            copy-generated-png-files-to-dat
)

### gen-mi-enum ################################################################
set(gen_mi_enum_output
    ${CMAKE_CURRENT_SOURCE_DIR}/mi-enum.h
)

add_custom_command(
    OUTPUT ${gen_mi_enum_output}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND util/gen-mi-enum
)

add_custom_target(gen-mi-enum DEPENDS ${gen_mi_enum_output})

### configure (generate config.h) ##############################################

set(generated_config
    ${CMAKE_CURRENT_BINARY_DIR}/config.h
)
add_custom_command(
    OUTPUT ${generated_config}
    COMMAND ${CMAKE_SOURCE_DIR}/source/util/configure
            --stdc++${CMAKE_CXX_STANDARD}
            ${CMAKE_CXX_COMPILER} ${CMAKE_CXX_FLAGS}
)
add_custom_target(generate-config DEPENDS ${generated_config})

add_library(generated-config INTERFACE)
target_include_directories(generated-config INTERFACE
    ${CMAKE_CURRENT_BINARY_DIR}
)
add_dependencies(generated-config generate-config)

### gen-cflg.pl ################################################################
# TODO Getting link and compile flags used to build is unfortunately a bit more
# complicated in cmake.

set(generated_compflag
    ${CMAKE_CURRENT_SOURCE_DIR}/compflag.h
)

add_custom_command(
    OUTPUT ${generated_compflag}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND util/gen-cflg.pl compflag.h
            "unknown" "unknown" "${HOST}" "${ARCH}"
)
add_custom_target(gen-cflg DEPENDS ${generated_compflag})

### gen_ver.pl #################################################################
set(generated_build
    ${CMAKE_CURRENT_SOURCE_DIR}/build.h
)
add_custom_command(
    OUTPUT ${generated_build}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND util/gen_ver.pl build.h "${MERGE_BASE}"
)
add_custom_target(gen-ver DEPENDS ${generated_build})

### gen .DES parser ############################################################
if(USE_PREBUILT_LEVCOMP)
    set(flex_output
        ${CMAKE_CURRENT_SOURCE_DIR}/prebuilt/levcomp.lex.cc)
    set(bison_output
        ${CMAKE_CURRENT_SOURCE_DIR}/prebuilt/levcomp.tab.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/prebuilt/levcomp.tab.h)
else()
    set(flex_output
        ${CMAKE_CURRENT_BINARY_DIR}/levcomp.lex.cc
    )
    add_custom_command(
        OUTPUT ${flex_output}
        COMMAND ${FLEX_EXECUTABLE} -o ${flex_output}
                ${CMAKE_CURRENT_SOURCE_DIR}/util/levcomp.lpp
    )

    set(bison_output
        ${CMAKE_CURRENT_BINARY_DIR}/levcomp.tab.cc
        ${CMAKE_CURRENT_BINARY_DIR}/levcomp.tab.h
    )
    add_custom_command(
        OUTPUT ${bison_output}
        COMMAND ${BISON_EXECUTABLE} -y
                --defines=${CMAKE_CURRENT_BINARY_DIR}/levcomp.tab.h
                -b levcomp -o ${CMAKE_CURRENT_BINARY_DIR}/levcomp.tab.cc
                ${CMAKE_CURRENT_SOURCE_DIR}/util/levcomp.ypp
    )
    add_custom_target(gen-lex DEPENDS ${flex_output} ${bison_output})
endif()

### gen-species ################################################################
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/generated/species-gen)
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/generated/species-gen/species-data.h
           ${CMAKE_BINARY_DIR}/generated/species-gen/aptitudes.h
           ${CMAKE_BINARY_DIR}/generated/species-gen/species-groups.h
           ${CMAKE_BINARY_DIR}/generated/species-gen/species-type.h
    COMMAND ${CMAKE_SOURCE_DIR}/source/util/species-gen.py
            ${CMAKE_SOURCE_DIR}/source/dat/species/
            ${CMAKE_SOURCE_DIR}/source/util/species-gen/
            species-data.h
            aptitudes.h
            species-groups.h
            species-type.h
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/generated/species-gen/
)

add_custom_target(gen-species-data
    DEPENDS
        ${CMAKE_BINARY_DIR}/generated/species-gen/species-data.h
        ${CMAKE_BINARY_DIR}/generated/species-gen/aptitudes.h
        ${CMAKE_BINARY_DIR}/generated/species-gen/species-groups.h
        ${CMAKE_BINARY_DIR}/generated/species-gen/species-type.h
)

add_library(species INTERFACE)
target_include_directories(species INTERFACE
    ${CMAKE_BINARY_DIR}/generated/species-gen
)
add_dependencies(generated-config generate-config)

### gen-doc ####################################################################
set(aptitudes_doc ${CMAKE_SOURCE_DIR}/docs/aptitudes.txt)
add_custom_command(
    OUTPUT ${aptitudes_doc}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/docs
    COMMAND ${CMAKE_SOURCE_DIR}/source/util/gen-apt.pl
        ${aptitudes_doc}
        ${CMAKE_SOURCE_DIR}/docs/template/apt-tmpl.txt
        ${CMAKE_BINARY_DIR}/generated/species-gen/species-data.h
        ${CMAKE_BINARY_DIR}/generated/species-gen/aptitudes.h
    DEPENDS
        ${CMAKE_SOURCE_DIR}/source/util/gen-apt.pl
        ${CMAKE_SOURCE_DIR}/docs/template/apt-tmpl.txt
        ${CMAKE_BINARY_DIR}/generated/species-gen/species-data.h
        ${CMAKE_BINARY_DIR}/generated/species-gen/aptitudes.h
)

set(aptitudes_wide_doc ${CMAKE_SOURCE_DIR}/docs/aptitudes-wide.txt)
add_custom_command(
    OUTPUT ${aptitudes_wide_doc}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/docs
    COMMAND ${CMAKE_SOURCE_DIR}/source/util/gen-apt.pl
        ${aptitudes_wide_doc}
        ${CMAKE_SOURCE_DIR}/docs/template/apt-tmpl-wide.txt
        ${CMAKE_BINARY_DIR}/generated/species-gen/species-data.h
        ${CMAKE_BINARY_DIR}/generated/species-gen/aptitudes.h
    DEPENDS
        ${CMAKE_SOURCE_DIR}/source/util/gen-apt.pl
        ${CMAKE_SOURCE_DIR}/docs/template/apt-tmpl-wide.txt
        ${CMAKE_BINARY_DIR}/generated/species-gen/species-data.h
        ${CMAKE_BINARY_DIR}/generated/species-gen/aptitudes.h
)

set(faq_doc ${CMAKE_SOURCE_DIR}/docs/FAQ.html)
add_custom_command(
    OUTPUT ${faq_doc}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/docs
    COMMAND ${CMAKE_SOURCE_DIR}/source/util/FAQ2html.pl
        ${CMAKE_SOURCE_DIR}/source/dat/database/FAQ.txt
        ${faq_doc}
    DEPENDS
        ${CMAKE_SOURCE_DIR}/source/util/FAQ2html.pl
        ${CMAKE_SOURCE_DIR}/source/dat/database/FAQ.txt
)

set(crawl_manual_doc ${CMAKE_SOURCE_DIR}/docs/crawl_manual.txt)
add_custom_command(
    OUTPUT ${crawl_manual_doc}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/docs
    COMMAND ${CMAKE_SOURCE_DIR}/source/util/unrest.pl
            ${CMAKE_SOURCE_DIR}/docs/crawl_manual.rst > ${crawl_manual_doc}
    VERBATIM
    DEPENDS
        ${CMAKE_SOURCE_DIR}/source/util/unrest.pl
        ${CMAKE_SOURCE_DIR}/docs/crawl_manual.rst
)

add_custom_target(copy-doc-to-stage
    COMMENT "Copy docs to stage"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/stage
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/docs docs
    DEPENDS ${aptitudes_doc}
            ${aptitudes_wide_doc}
            ${faq_doc}
            ${crawl_manual_doc}
)

################################################################################
### CRAWL
################################################################################
set(target crawl)

# Add headerfiles so that CMake aware IDEs or generators for MSVC
# will show our headers
set(header_files
    ability-type.h
    ability.h
    abyss.h
    ac-type.h
    acquire.h
    act-iter.h
    activity-interrupt-type.h
    actor.h
    adjust.h
    AppHdr.h
    areas.h
    arena.h
    art-func.h
    artefact-prop-type.h
    artefact.h
    attack.h
    attitude-change.h
    attribute-type.h
    beam-type.h
    beam.h
    beh-type.h
    bitary.h
    bloodspatter.h
    book-data.h
    book-type.h
    branch-data.h
    branch-type.h
    branch.h
    butcher.h
    caction-type.h
    canned-message-type.h
    char-set-type.h
    chardump.h
    cio.h
    cleansing-flame-source-type.h
    cloud-type.h
    cloud.h
    clua.h
    cluautil.h
    cmd-keys.h
    colour.h
    command-type.h
    command.h
    conduct-type.h
    confirm-butcher-type.h
    confirm-prompt-type.h
    coord-circle.h
    coord.h
    coordit.h
    crash.h
    ctest.h
    cursor-type.h
    daction-type.h
    dactions.h
    database.h
    dbg-maps.h
    dbg-objstat.h
    dbg-scan.h
    dbg-util.h
    debug.h
    decks.h
    defines.h
    delay.h
    describe-god.h
    describe-spells.h
    describe.h
    description-level-type.h
    dgl-message.h
    dgn-delve.h
    dgn-event.h
    dgn-height.h
    dgn-irregular-box.h
    dgn-layouts.h
    dgn-overview.h
    dgn-proclayouts.h
    dgn-shoals.h
    dgn-swamp.h
    directn.h
    disable-type.h
    dlua.h
    domino-data.h
    domino.h
    dungeon-char-type.h
    dungeon-feature-type.h
    dungeon.h
    duration-data.h
    duration-type.h
    enchant-type.h
    end.h
    endianness.h
    energy-use-type.h
    english.h
    enum.h
    env.h
    eq-type-flags.h
    eq-type.h
    equipment-type.h
    errors.h
    evoke.h
    exclude.h
    exercise.h
    externs.h
    feature-data.h
    feature.h
    fight.h
    files.h
    fineff.h
    fixedarray.h
    fixedvector.h
    flang-t.h
    flood-find.h
    flush-reason-type.h
    fontwrapper-ft.h
    food.h
    form-data.h
    format.h
    fprop.h
    game-chapter.h
    game-exit-type.h
    game-options.h
    game-type.h
    gender-type.h
    geom2d.h
    ghost.h
    glwrapper-ogl.h
    glwrapper.h
    god-abil.h
    god-blessing.h
    god-companions.h
    god-conduct.h
    god-item.h
    god-menu.h
    god-passive.h
    god-prayer.h
    god-type.h
    god-wrath.h
    hash.h
    hints.h
    hiscores.h
    holy-word-source-type.h
    hunger-state-t.h
    initfile.h
    invent.h
    item-name.h
    item-prop-enum.h
    item-prop.h
    item-status-flag-type.h
    item-type-id-state-type.h
    item-use.h
    items.h
    job-data.h
    job-type.h
    jobs.h
    json-wrapper.h
    json.h
    KeymapContext.h
    kill-category.h
    killer-type.h
    kills.h
    l-defs.h
    l-libs.h
    lang-fake.h
    lang-t.h
    lev-pand.h
    level-state-type.h
    libconsole.h
    $<$<NOT:$<STREQUAL:${TILE_MODE},LOCAL_TILES>>:libunix.h>
    libutil.h
    libw32c.h
    loading-screen.h
    lookup-help.h
    los-def.h
    los-type.h
    los.h
    losglobal.h
    losparam.h
    luaterp.h
    macro.h
    makeitem.h
    map-cell.h
    map-feature.h
    map-knowledge.h
    map-marker-type.h
    mapdef.h
    mapmark.h
    maps.h
    matrix.h
    maybe-bool.h
    melee-attack.h
    menu-type.h
    menu.h
    message.h
    mgen-data.h
    mgen-enum.h
    misc.h
    mon-abil.h
    mon-act.h
    mon-attitude-type.h
    mon-behv.h
    mon-book.h
    mon-cast.h
    mon-clone.h
    mon-data.h
    mon-death.h
    mon-ench.h
    mon-enum.h
    mon-flags.h
    mon-gear.h
    mon-grow.h
    mon-holy-type.h
    mon-info.h
    mon-inv-type.h
    mon-movetarget.h
    mon-pathfind.h
    mon-pick-data.h
    mon-pick.h
    mon-place.h
    mon-poly.h
    mon-project.h
    mon-speak.h
    mon-spell.h
    mon-tentacle.h
    mon-transit.h
    mon-util.h
    monster-type.h
    monster.h
    montravel-target-type.h
    movement.h
    mpr.h
    msvc.h
    mutant-beast.h
    mutation-data.h
    mutation-type.h
    mutation.h
    nearby-danger.h
    newgame-def.h
    newgame.h
    ng-init.h
    ng-input.h
    ng-restr.h
    ng-setup.h
    ng-wanderer.h
    noise.h
    notes.h
    object-class-type.h
    operation-types.h
    options.h
    orb-type.h
    orb.h
    ouch.h
    output.h
    package.h
    pattern.h
    pcg.h
    perlin.h
    place-info.h
    place.h
    platform.h
    playable.h
    player-equip.h
    player-reacts.h
    player-save-info.h
    player-stats.h
    player.h
    potion-type.h
    potion.h
    prompt.h
    pronoun-type.h
    props.h
    quiver.h
    randbook.h
    random-pick.h
    random-var.h
    random.h
    ranged-attack.h
    ray.h
    reach-type.h
    recite-eligibility.h
    recite-type.h
    religion-enum.h
    religion.h
    rng-type.h
    rot.h
    sacrifice-data.h
    score-format-type.h
    screen-mode.h
    scroller.h
    SDLMain.h
    seen-context-type.h
    sense-type.h
    shop-type.h
    shopping.h
    shout.h
    show.h
    showsymb.h
    size-part-type.h
    size-type.h
    skill-focus-mode.h
    skill-menu-state.h
    skill-menu.h
    skill-type.h
    skills.h
    slot-select-mode.h
    sound.h
    species-def.h
    species.h
    spell-type.h
    spl-book.h
    spl-cast.h
    spl-clouds.h
    spl-damage.h
    spl-data.h
    spl-goditem.h
    spl-miscast.h
    spl-monench.h
    spl-other.h
    spl-pick.h
    spl-selfench.h
    spl-summoning.h
    spl-transloc.h
    spl-util.h
    spl-wpnench.h
    spl-zap.h
    sprint.h
    sqldbm.h
    stairs.h
    startup.h
    stash.h
    stat-type.h
    state.h
    status.h
    stepdown.h
    store.h
    stringutil.h
    syscalls.h
    tag-pref.h
    tag-version.h
    tags.h
    targ-mode-type.h
    target.h
    targeting-type.h
    teleport.h
    terrain-change-type.h
    terrain.h
    text-tag-type.h
    threads.h
    throw.h
    tile-flags.h
    tile-inventory-flags.h
    tile-player-flag-cut.h
    tile-player-flags.h
    tilebuf.h
    tilecell.h
    tiledgnbuf.h
    tiledoll.h
    tilefont.h
    tilemcache.h
    tilepick-p.h
    tilepick.h
    tilereg-abl.h
    tilereg-cmd.h
    tilereg-crt.h
    tilereg-dgn.h
    tilereg-doll.h
    tilereg-grid.h
    tilereg-inv.h
    tilereg-map.h
    tilereg-mem.h
    tilereg-mon.h
    tilereg-msg.h
    tilereg-skl.h
    tilereg-spl.h
    tilereg-stat.h
    tilereg-tab.h
    tilereg-text.h
    tilereg.h
    tiles-build-specific.h
    tiles.h
    tilesdl.h
    tiletex.h
    tileview.h
    tileweb-text.h
    tileweb.h
    timed-effect-type.h
    timed-effects.h
    torment-source-type.h
    transform.h
    transformation.h
    trap-def.h
    trap-type.h
    traps.h
    travel-defs.h
    travel.h
    tutorial.h
    ui.h
    uncancel.h
    uncancellable-type.h
    undead-state-type.h
    unicode.h
    unique-item-status-type.h
    unwind.h
    version.h
    view.h
    viewchar.h
    viewgeom.h
    viewmap.h
    windowmanager-sdl.h
    windowmanager.h
    wiz-dgn.h
    wiz-dump.h
    wiz-fsim.h
    wiz-item.h
    wiz-mon.h
    wiz-you.h
    wizard-option-type.h
    wizard.h
    worley.h
    wu-jian-attack-type.h
    xom.h
    xp-tracking-type.h
    zap-data.h
    zap-type.h
)

set(source_files
    ability.cc
    abyss.cc
    acquire.cc
    act-iter.cc
    actor-los.cc
    actor.cc
    adjust.cc
    areas.cc
    arena.cc
    artefact.cc
    attack.cc
    attitude-change.cc
    beam.cc
    behold.cc
    bitary.cc
    bloodspatter.cc
    branch.cc
    butcher.cc
    chardump.cc
    cio.cc
    cloud.cc
    clua.cc
    cluautil.cc
    colour.cc
    command.cc
    coord-circle.cc
    coord.cc
    coordit.cc
    crash.cc
    ctest.cc
    dactions.cc
    database.cc
    dbg-asrt.cc
    dbg-maps.cc
    dbg-objstat.cc
    dbg-scan.cc
    dbg-util.cc
    decks.cc
    delay.cc
    describe-god.cc
    describe-spells.cc
    describe.cc
    dgl-message.cc
    dgn-delve.cc
    dgn-event.cc
    dgn-height.cc
    dgn-irregular-box.cc
    dgn-layouts.cc
    dgn-overview.cc
    dgn-proclayouts.cc
    dgn-shoals.cc
    dgn-swamp.cc
    directn.cc
    dlua.cc
    domino.cc
    dungeon.cc
    end.cc
    english.cc
    errors.cc
    evoke.cc
    exclude.cc
    exercise.cc
    fearmonger.cc
    feature.cc
    fight.cc
    files.cc
    fineff.cc
    fontwrapper-ft.cc
    food.cc
    format.cc
    fprop.cc
    game-options.cc
    geom2d.cc
    ghost.cc
    glwrapper-ogl.cc
    glwrapper.cc
    god-abil.cc
    god-blessing.cc
    god-companions.cc
    god-conduct.cc
    god-item.cc
    god-menu.cc
    god-passive.cc
    god-prayer.cc
    god-wrath.cc
    hash.cc
    hints.cc
    hiscores.cc
    initfile.cc
    invent.cc
    item-name.cc
    item-prop.cc
    item-use.cc
    items.cc
    jobs.cc
    json.cc
    kills.cc
    l-colour.cc
    l-crawl.cc
    l-debug.cc
    l-dgn.cc
    l-dgnbld.cc
    l-dgnevt.cc
    l-dgngrd.cc
    l-dgnit.cc
    l-dgnlvl.cc
    l-dgnmon.cc
    l-dgntil.cc
    l-feat.cc
    l-file.cc
    l-food.cc
    l-global.cc
    l-item.cc
    l-los.cc
    l-mapgrd.cc
    l-mapmrk.cc
    l-moninf.cc
    l-mons.cc
    l-option.cc
    l-spells.cc
    l-subvault.cc
    l-travel.cc
    l-view.cc
    l-wiz.cc
    l-you.cc
    lang-fake.cc
    lev-pand.cc
    libgui.cc
    $<$<NOT:$<STREQUAL:${TILE_MODE},LOCAL_TILES>>:libunix.cc>
    libutil.cc
    # TODO windows
    # libw32c.cc
    loading-screen.cc
    lookup-help.cc
    los-def.cc
    los.cc
    losglobal.cc
    losparam.cc
    luaterp.cc
    macro.cc
    main.cc
    makeitem.cc
    map-knowledge.cc
    mapdef.cc
    mapmark.cc
    maps.cc
    melee-attack.cc
    menu.cc
    message-stream.cc
    message.cc
    misc.cc
    mon-abil.cc
    mon-act.cc
    mon-behv.cc
    mon-cast.cc
    mon-clone.cc
    mon-death.cc
    mon-ench.cc
    mon-gear.cc
    mon-grow.cc
    mon-info.cc
    mon-movetarget.cc
    mon-pathfind.cc
    mon-pick.cc
    mon-place.cc
    mon-poly.cc
    mon-project.cc
    mon-speak.cc
    mon-tentacle.cc
    mon-transit.cc
    mon-util.cc
    monster.cc
    movement.cc
    mutation.cc
    nearby-danger.cc
    newgame.cc
    ng-init.cc
    ng-input.cc
    ng-restr.cc
    ng-setup.cc
    ng-wanderer.cc
    notes.cc
    orb.cc
    ouch.cc
    output.cc
    package.cc
    pattern.cc
    pcg.cc
    perlin.cc
    place-info.cc
    place.cc
    playable.cc
    player-act.cc
    player-equip.cc
    player-reacts.cc
    player-stats.cc
    player.cc
    potion.cc
    prompt.cc
    quiver.cc
    randbook.cc
    random-var.cc
    random.cc
    ranged-attack.cc
    ray.cc
    religion.cc
    rot.cc
    scroller.cc
    shopping.cc
    shout.cc
    show.cc
    showsymb.cc
    skill-menu.cc
    skills.cc
    sound.cc
    species.cc
    spl-book.cc
    spl-cast.cc
    spl-clouds.cc
    spl-damage.cc
    spl-goditem.cc
    spl-miscast.cc
    spl-monench.cc
    spl-other.cc
    spl-pick.cc
    spl-selfench.cc
    spl-summoning.cc
    spl-tornado.cc
    spl-transloc.cc
    spl-util.cc
    spl-wpnench.cc
    spl-zap.cc
    sprint.cc
    sqldbm.cc
    stairs.cc
    startup.cc
    stash.cc
    state.cc
    status.cc
    stepdown.cc
    store.cc
    stringutil.cc
    syscalls.cc
    tags.cc
    target.cc
    teleport.cc
    terrain.cc
    throw.cc
    tilebuf.cc
    tilecell.cc
    tiledgnbuf.cc
    tiledoll.cc
    tilefont.cc
    tilemcache.cc
    tilepick-p.cc
    tilepick.cc
    tilereg-abl.cc
    tilereg-cmd.cc
    tilereg-crt.cc
    tilereg-dgn.cc
    tilereg-doll.cc
    tilereg-grid.cc
    tilereg-inv.cc
    tilereg-map.cc
    tilereg-mem.cc
    tilereg-mon.cc
    tilereg-msg.cc
    tilereg-skl.cc
    tilereg-spl.cc
    tilereg-stat.cc
    tilereg-tab.cc
    tilereg-text.cc
    tilereg.cc
    tilesdl.cc
    tiletex.cc
    tileview.cc
    tileweb-text.cc
    tileweb.cc
    timed-effects.cc
    transform.cc
    traps.cc
    travel.cc
    tutorial.cc
    ui.cc
    uncancel.cc
    unicode.cc
    version.cc
    view.cc
    viewchar.cc
    viewgeom.cc
    viewmap.cc
    wcwidth.cc
    windowmanager-sdl.cc
    wiz-dgn.cc
    wiz-dump.cc
    wiz-fsim.cc
    wiz-item.cc
    wiz-mon.cc
    wiz-you.cc
    wizard.cc
    worley.cc
    xom.cc
)

set(CROSSBUILD OFF)
set(configuration_specific
    $<$<BOOL:CROSSBUILD>:AppHdr.cc>
)

add_executable(${target}
    ${flex_output}
    ${bison_output}
    ${header_files}
    ${source_files}
    ${configuration_specific}
    ${art_data_output}
    ${art_data_output_headers}
)

target_include_directories(${target}
    PUBLIC
        .
        prebuild
)

target_link_libraries(${target} PUBLIC
    lua
    $<$<NOT:$<STREQUAL:${TILE_MODE},LOCAL_TILES>>:curses>
    $<$<STREQUAL:${TILE_MODE},LOCAL_TILES>:sdl2>
    $<$<STREQUAL:${TILE_MODE},LOCAL_TILES>:sdl2-image>
    $<$<STREQUAL:${TILE_MODE},LOCAL_TILES>:OpenGL::GL>
    $<$<STREQUAL:${TILE_MODE},LOCAL_TILES>:OpenGL::GLU>
    $<$<STREQUAL:${TILE_MODE},LOCAL_TILES>:freetype>
    sqlite
    generated-config
    Threads::Threads
    rltiles
    species
)

target_compile_options(${target}
    PUBLIC
    $<$<CXX_COMPILER_ID:AppleClang>:-Wno-logical-op-parentheses>
)

target_compile_definitions(${target}
    PUBLIC
        ${COMMON_DEFINES}
)

set_target_properties(${target} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/stage"
)

add_dependencies(${target}
    art-data
    gen-mst
    cmd-name
    gen-mi-enum
    gen-cflg
    gen-ver
    copy-doc-to-stage
    copy-settings-to-stage
    copy-dat-to-stage
)

add_custom_target(copy-settings-to-stage
    COMMENT "Copy settings to stage"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/stage
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/settings settings
)

################################################################################
# Setup stage folder when building WEB_TILES
# TODO: only move js/png files where they are actually needed
if(TILE_MODE STREQUAL WEB_TILES)
    file(MAKE_DIRECTORY
        ${CMAKE_BINARY_DIR}/stage/util
        ${CMAKE_BINARY_DIR}/stage/rcs
    )
    add_custom_command(
        COMMENT "Copy files for webserver"
        OUTPUT copy_webserver_symbolic
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                webserver
                ${CMAKE_BINARY_DIR}/stage/webserver
        COMMAND ${CMAKE_COMMAND} -E copy
                util/webtiles-init-player-for-cmake.sh
                ${CMAKE_BINARY_DIR}/stage/util/webtiles-init-player.sh
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_BINARY_DIR}/stage/dat/tiles
                ${CMAKE_BINARY_DIR}/stage/webserver/static
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_BINARY_DIR}/stage/dat/tiles
                ${CMAKE_BINARY_DIR}/stage/webserver/game_data/static
        COMMAND ${CMAKE_COMMAND} -E copy
                rltiles/*.js
                ${CMAKE_BINARY_DIR}/stage/webserver/game_data/static
        DEPENDS copy-dat-to-stage
    )
    set_source_files_properties(copy_webserver_symbolic
        PROPERTIES SYMBOLIC TRUE
    )

    add_custom_target(relocate-files-for-webserver
        DEPENDS copy_webserver_symbolic
    )

    add_dependencies(${target} relocate-files-for-webserver)
endif()
