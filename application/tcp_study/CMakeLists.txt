set(TCP_STUDY_PROGRAM_A tcp_study_program_a)
set(TCP_STUDY_PROGRAM_B tcp_study_program_b)

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
