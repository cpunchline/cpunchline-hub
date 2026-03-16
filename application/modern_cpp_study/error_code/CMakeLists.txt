set(ERROR_CODE_TEST error_code_test)

add_executable(${ERROR_CODE_TEST}
    error_code.cpp
)
target_include_directories(${ERROR_CODE_TEST} PRIVATE .)

install(TARGETS ${ERROR_CODE_TEST} DESTINATION test)
