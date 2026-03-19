set(TCP_STUDY_PROGRAM_A tcp_study_program_a)
set(TCP_STUDY_PROGRAM_B tcp_study_program_b)
set(TCP_TEMPEST_PROGRAM tcp_tempest_program) # https://bgithub.xyz/chenshuo/tempest/blob/master/tempest.cc

add_executable(${TCP_STUDY_PROGRAM_A}
    ${TCP_STUDY_PROGRAM_A}.c
)
target_include_directories(${TCP_STUDY_PROGRAM_A} PRIVATE .)

install(TARGETS ${TCP_STUDY_PROGRAM_A} DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable(${TCP_STUDY_PROGRAM_B}
    ${TCP_STUDY_PROGRAM_B}.c
)
target_include_directories(${TCP_STUDY_PROGRAM_B} PRIVATE .)

install(TARGETS ${TCP_STUDY_PROGRAM_B} DESTINATION ${CMAKE_INSTALL_BINDIR})

find_library(READLINE_LIBRARY NAMES readline)
# /usr/lib/x86_64-linux-gnu/libreadline.so
# /usr/include/readline/*.h

if(READLINE_LIBRARY)
    add_executable(${TCP_TEMPEST_PROGRAM}
        ${TCP_TEMPEST_PROGRAM}.cpp
    )
    target_include_directories(${TCP_TEMPEST_PROGRAM} PRIVATE .)
    target_link_libraries(${TCP_TEMPEST_PROGRAM} PRIVATE readline)
    target_compile_options(${TCP_TEMPEST_PROGRAM} PRIVATE -w)

    install(TARGETS ${TCP_TEMPEST_PROGRAM} DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
