#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "nanopb::protobuf-nanopb" for configuration "Release"
set_property(TARGET nanopb::protobuf-nanopb APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(nanopb::protobuf-nanopb PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libprotobuf-nanopb.so.0"
  IMPORTED_SONAME_RELEASE "libprotobuf-nanopb.so.0"
  )

list(APPEND _cmake_import_check_targets nanopb::protobuf-nanopb )
list(APPEND _cmake_import_check_files_for_nanopb::protobuf-nanopb "${_IMPORT_PREFIX}/lib/libprotobuf-nanopb.so.0" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
