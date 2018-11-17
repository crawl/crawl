set(target fake_pty)

set(source_files
    fake_pty.c
)

add_executable(${target}
    ${source_files}
)

#TODO Add travis support
if(TARVIS)
    set(timeout "TIMEOUT=9")
else()
    set(timeout "TIMEOUT=60")
endif()


target_compile_definitions(${target}
    PUBLIC
        ${timeout}
)

find_library(UTIL util)
if(NOT UTIL)
    message(FATAL_ERROR "Cannot find libutil.")
endif()

target_link_libraries(${target}
    PUBLIC
        ${UTIL}
)
