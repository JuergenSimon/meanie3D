FIND_PATH(SHP_INCLUDE_DIR shapefil.h PATHS /usr/include /usr/local/include /opt/local/include ./shapelib-1.3.0)
FIND_LIBRARY(SHP NAMES shp PATHS /usr/lib /usr/local/lib /opt/local/lib ~/radolan ./shapelib-1.3.0)

IF (SHP)
   SET(SHP_LIBRARIES ${SHP})
ELSE ()
   SET(SHP_LIBRARIES "")
ENDIF ()

IF (SHP_INCLUDE_DIR AND SHP_LIBRARIES)
   SET(SHP_FOUND TRUE)
ENDIF ()

IF (NOT SHP_FOUND AND SHP_FIND_REQUIRED)
   MESSAGE(FATAL_ERROR "Could not find SHP")
ENDIF ()

MARK_AS_ADVANCED(SHP_LIBRARIES SHP_INCLUDE_DIR)