FIND_PATH(FLANN_INCLUDE_DIR flann PATHS /usr/include /usr/local/include/ opt/local/include)
FIND_LIBRARY(FLANN NAMES flann PATHS /usr/lib /usr/local/lib /opt/local/lib)

IF (FLANN_INCLUDE_DIR AND FLANN)
   SET(FLANN_LIBRARIES ${FLANN})
   SET(FLANN_FOUND TRUE)
ELSE (FLANN_INCLUDE_DIR AND FLANN)
   SET(FLANN_LIBRARIES "NOTFOUND")
   SET(FLANN_FOUND FALSE)
ENDIF(FLANN_INCLUDE_DIR AND FLANN)

IF (FLANN_FOUND)
   IF (NOT FLANN_FIND_QUIETLY)
      MESSAGE(STATUS "Found FLANN: ${FLANN_LIBRARIES}")
   ENDIF (NOT FLANN_FIND_QUIETLY)
ELSE (FLANN_FOUND)
   IF (FLANN_FIND_REQUIRED)
      MESSAGE(FATAL_ERROR "Could not find FLANN")
   ENDIF (FLANN_FIND_REQUIRED)
ENDIF (FLANN_FOUND)