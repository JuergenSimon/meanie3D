#
# - Find Radolan library
#
# The libradolan library is required if the adaptor for 
# radolan data should be created. 
# 
# Output Variables:
#  libradolan_INCLUDE_DIR where to find radolan.h
#  libradolan_LIBRARIES location of libradolan.so/.dylib
#  libradolan_FOUND flag indicating if both include and library have been located

FIND_PATH(libradolan_INCLUDE_DIR radolan PATHS /usr/include /usr/local/include /opt/local/include)
FIND_LIBRARY(libradolan_LIBRARY NAMES radolan)

IF (libradolan_INCLUDE_DIR AND libradolan_LIBRARY)
   SET(libradolan_FOUND TRUE)
ENDIF ()

# handle the QUIETLY and REQUIRED arguments and set libradolan_FOUND to TRUE if 
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(libradolan DEFAULT_MSG libradolan_LIBRARY libradolan_INCLUDE_DIR)

IF(libradolan_FOUND)
    SET(libradolan_LIBRARIES ${libradolan_LIBRARY} )
    SET(libradolan_INCLUDE_DIRS ${libradolan_INCLUDE_DIR} )
ELSEIF (libradolan_FIND_REQUIRED)
    MESSAGE(FATAL_ERROR "libradolan not found")
ENDIF()

MARK_AS_ADVANCED(libradolan_INCLUDE_DIR libradolan_LIBRARY)