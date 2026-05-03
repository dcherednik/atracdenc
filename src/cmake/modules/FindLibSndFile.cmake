
if (LIBSNDFILE_INCLUDE_DIR AND SNDFILE_LIBRARY)
    # Already in cache, be silent
    set(LIBSNDFILE_FIND_QUIETLY TRUE)
endif (LIBSNDFILE_INCLUDE_DIR AND SNDFILE_LIBRARY)

set(_LIBSNDFILE_HINT_PATHS)
if (DEFINED ENV{MINGW_PREFIX})
    list(APPEND _LIBSNDFILE_HINT_PATHS "$ENV{MINGW_PREFIX}")
endif()

find_path(LIBSNDFILE_INCLUDE_DIR sndfile.h
    HINTS ${_LIBSNDFILE_HINT_PATHS}
    PATH_SUFFIXES include
    PATHS /usr/include /opt/local/include/
)

find_library(SNDFILE_LIBRARY
    NAMES sndfile
    HINTS ${_LIBSNDFILE_HINT_PATHS}
    PATH_SUFFIXES lib
    PATHS /usr/lib /usr/local/lib /opt/local/lib
)

set(SNDFILE_LIBRARIES ${SNDFILE_LIBRARY})

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(LIBSNDFILE
        DEFAULT_MSG
        LIBSNDFILE_INCLUDE_DIR
        SNDFILE_LIBRARIES
)

mark_as_advanced(LIBSNDFILE_INCLUDE_DIR SNDFILE_LIBRARY)
