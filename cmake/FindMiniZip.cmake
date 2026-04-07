find_path(MINIZIP_INCLUDE_DIR
    NAMES minizip/unzip.h unzip.h
    PATH_SUFFIXES include
)

find_library(MINIZIP_LIBRARY
    NAMES minizip minizip-ng-compat
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MiniZip DEFAULT_MSG MINIZIP_LIBRARY MINIZIP_INCLUDE_DIR)

if(MINIZIP_FOUND AND NOT TARGET MiniZip::MiniZip)
    add_library(MiniZip::MiniZip UNKNOWN IMPORTED)
    set_target_properties(MiniZip::MiniZip PROPERTIES
        IMPORTED_LOCATION "${MINIZIP_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${MINIZIP_INCLUDE_DIR}"
    )
endif()
