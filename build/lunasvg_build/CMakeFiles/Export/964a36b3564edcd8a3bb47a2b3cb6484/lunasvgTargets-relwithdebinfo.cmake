#----------------------------------------------------------------
# Generated CMake target import file for configuration "RelWithDebInfo".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "lunasvg::lunasvg" for configuration "RelWithDebInfo"
set_property(TARGET lunasvg::lunasvg APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(lunasvg::lunasvg PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELWITHDEBINFO "CXX"
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/lib/lunasvg.lib"
  )

list(APPEND _cmake_import_check_targets lunasvg::lunasvg )
list(APPEND _cmake_import_check_files_for_lunasvg::lunasvg "${_IMPORT_PREFIX}/lib/lunasvg.lib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
