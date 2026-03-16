#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "hv" for configuration "Release"
set_property(TARGET hv APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(hv PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libhv.so"
  IMPORTED_SONAME_RELEASE "libhv.so"
  )

list(APPEND _cmake_import_check_targets hv )
list(APPEND _cmake_import_check_files_for_hv "${_IMPORT_PREFIX}/lib/libhv.so" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
