project(avogadro)

if(WIN32)
  add_definitions( -DWIN32 )
  # add definitions for OB in WIN32
  add_definitions( -DUSING_OBDLL -DUSING_DYNAMIC_LIBS )
endif()

# Ensure the Avogadro include directory is always first
get_directory_property(tmp_include_dirs INCLUDE_DIRECTORIES)
set_directory_properties(PROPERTIES INCLUDE_DIRECTORIES
  "${libavogadro_BINARY_DIR}/include;${tmp_include_dirs}")
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${OPENBABEL2_INCLUDE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
)


# tell cmake to process CMakeLists.txt in that subdirectory
add_subdirectory(src)
#include(src/CMakeLists.txt)
