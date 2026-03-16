set(SQLITE_STUDY sqlite_study)

find_package_from_path(SQLite3 ${CPUNCHLINE_PRE_BUILT_DIR})

add_executable(${SQLITE_STUDY}
    sqlite3_study.c
)
target_link_libraries(${SQLITE_STUDY} PRIVATE SQLite::SQLite3 base)

install(TARGETS ${SQLITE_STUDY} DESTINATION ${CMAKE_INSTALL_BINDIR})
