FIND_PATH(SHP_INCLUDE_DIR shapefil.h PATHS /usr/include /usr/local/include /opt/local/include ./shapelib-1.3.0)
FIND_LIBRARY(SHP NAMES shp PATHS /usr/lib /usr/local/lib /opt/local/lib ~/radolan ./shapelib-1.3.0)

IF (SHP)
   SET(SHP_LIBRARIES ${SHP})
ELSE (SHP)
   SET(SHP_LIBRARIES "")
ENDIF(SHP)

IF (SHP_INCLUDE_DIR AND SHP_LIBRARIES)
   SET(SHP_FOUND TRUE)
ENDIF (SHP_INCLUDE_DIR AND SHP_LIBRARIES)

IF (SHP_FOUND)
   IF (NOT SHP_FIND_QUIETLY)
      MESSAGE(STATUS "Found SHP: ${SHP_LIBRARIES}")
   ENDIF (NOT SHP_FIND_QUIETLY)
ELSE (SHP_FOUND)
   IF (SHP_FIND_REQUIRED)
      MESSAGE(FATAL_ERROR "Could not find SHP")
   ENDIF (SHP_FIND_REQUIRED)
ENDIF (SHP_FOUND)