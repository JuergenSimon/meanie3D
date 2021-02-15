# - Find Blitz library
# Find the native Blitz includes and library
# This module defines
#  Blitz_INCLUDE_DIR, where to find tiff.h, etc.
#  Blitz_LIBRARIES, libraries to link against to use Blitz.
#  Blitz_FOUND, If false, do not try to use Blitz.
# also defined, but not for general use are
#  Blitz_LIBRARY, where to find the Blitz library.

FIND_PATH(Blitz_INCLUDE_DIR blitz PATHS /usr/include /usr/local/include /opt/local/include)
FIND_LIBRARY(Blitz_LIBRARY NAMES blitz)

IF (Blitz_INCLUDE_DIR AND Blitz_LIBRARY)
   SET(Blitz_FOUND TRUE)
ENDIF ()

# handle the QUIETLY and REQUIRED arguments and set Blitz_FOUND to TRUE if 
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Blitz DEFAULT_MSG Blitz_LIBRARY Blitz_INCLUDE_DIR)

IF (Blitz_FOUND)
    SET(Blitz_LIBRARIES ${Blitz_LIBRARY})
    SET(Blitz_INCLUDE_DIRS ${Blitz_INCLUDE_DIR})
ELSEIF (BLITZ_FIND_REQUIRED)
    MESSAGE(FATAL_ERROR "Could not find blitz++")
ENDIF ()

MARK_AS_ADVANCED(Blitz_INCLUDE_DIR Blitz_LIBRARY)



