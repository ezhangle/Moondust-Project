# Major
set(LAZYFIXTOOL_VERSION_1 3)
# Minor
set(LAZYFIXTOOL_VERSION_2 0)
# Revision
set(LAZYFIXTOOL_VERSION_3 0)
# Patch
set(LAZYFIXTOOL_VERSION_4 0)
# Type of version: "-alpha","-beta","-dev", or "" aka "release"
set(LAZYFIXTOOL_VERSION_REL "")

add_definitions(-DLAZYFIXTOOL_VERSION_1=${LAZYFIXTOOL_VERSION_1})
add_definitions(-DLAZYFIXTOOL_VERSION_2=${LAZYFIXTOOL_VERSION_2})
add_definitions(-DLAZYFIXTOOL_VERSION_3=${LAZYFIXTOOL_VERSION_3})
add_definitions(-DLAZYFIXTOOL_VERSION_4=${LAZYFIXTOOL_VERSION_4})
add_definitions(-DLAZYFIXTOOL_VERSION_REL=${LAZYFIXTOOL_VERSION_REL})

set(LAZYFIXTOOL_VERSION_STRING "${LAZYFIXTOOL_VERSION_1}.${LAZYFIXTOOL_VERSION_2}")

if(NOT ${LAZYFIXTOOL_VERSION_3} EQUAL 0 OR NOT ${LAZYFIXTOOL_VERSION_4} EQUAL 0)
    string(CONCAT LAZYFIXTOOL_VERSION_STRING "${LAZYFIXTOOL_VERSION_STRING}" ".${LAZYFIXTOOL_VERSION_3}")
endif()

if(NOT ${LAZYFIXTOOL_VERSION_4} EQUAL 0)
    string(CONCAT LAZYFIXTOOL_VERSION_STRING "${LAZYFIXTOOL_VERSION_STRING}" ".${LAZYFIXTOOL_VERSION_4}")
endif()

if(NOT "${LAZYFIXTOOL_VERSION_REL}" STREQUAL "")
    string(CONCAT LAZYFIXTOOL_VERSION_STRING "${LAZYFIXTOOL_VERSION_STRING}" "${LAZYFIXTOOL_VERSION_REL}")
endif()

message("== LazyFixTool version ${LAZYFIXTOOL_VERSION_STRING} ==")
