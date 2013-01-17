FIND_PATH(RADOLAN_INCLUDE_DIR radolan.h PATHS /usr/include/radolan /usr/local/include/radolan /opt/local/include/radolan)

FIND_LIBRARY(RADOLAN NAMES radolan PATHS /usr/lib /usr/local/lib /opt/local/lib)

IF (RADOLAN)
   SET(RADOLAN_LIBRARIES ${RADOLAN})
ELSE (RADOLAN)
   SET(RADOLAN_LIBRARIES "NOTFOUND")
ENDIF(RADOLAN)

IF (RADOLAN_INCLUDE_DIR AND RADOLAN_LIBRARIES)
   SET(RADOLAN_FOUND TRUE)
ENDIF (RADOLAN_INCLUDE_DIR AND RADOLAN_LIBRARIES)

IF (RADOLAN_FOUND)
   IF (NOT RADOLAN_FIND_QUIETLY)
      MESSAGE(STATUS "Found RADOLAN: ${RADOLAN_LIBRARIES}")
   ENDIF (NOT RADOLAN_FIND_QUIETLY)
ELSE (RADOLAN_FOUND)
   IF (RADOLAN_FIND_REQUIRED)
      MESSAGE(FATAL_ERROR "Could not find RADOLAN")
   ENDIF (RADOLAN_FIND_REQUIRED)
ENDIF (RADOLAN_FOUND)