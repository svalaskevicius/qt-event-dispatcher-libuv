# FindLibUV
#
# Find LibUV headers and library
#
#  LIBUV_FOUND          - True if libuv is found.
#  LIBUV_INCLUDE_DIRS   - Directory where libuv headers are located.
#  LIBUV_LIBRARIES      - Libuv libraries to link against.
#=============================================================================

find_path(LIBUV_INCLUDE_DIR uv.h)
find_library(LIBUV_LIBRARY uv)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LIBUV REQUIRED_VARS LIBUV_INCLUDE_DIR
                                                      LIBUV_LIBRARY
)

if(LIBUV_FOUND)
    set(LIBUV_INCLUDE_DIRS ${LIBUV_INCLUDE_DIR})
    set(LIBUV_LIBRARIES ${LIBUV_LIBRARY})

    if(NOT TARGET LibUV::LibUV)
      add_library(LibUV::LibUV UNKNOWN IMPORTED)
      set_target_properties(LibUV::LibUV PROPERTIES
        IMPORTED_LOCATION "${LIBUV_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${LIBUV_INCLUDE_DIRS}")
    endif()
endif(LIBUV_FOUND)

mark_as_advanced(LIBUV_INCLUDE_DIR LIBUV_LIBRARY)
