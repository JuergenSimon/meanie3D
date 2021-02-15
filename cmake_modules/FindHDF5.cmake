# CMAKE module for locating HDF5.                                                                                                                                                                                     
# Options: -DWITH_HDF5=<path to include root>                                                                                                                                                                         
# Example: -DWITH_HDF5=/usr/local                                                                                                                                                                                     

SET(HDF5_ROOT ${WITH_HDF5})
IF (PRESET MATCHES "^docker.*")
    MESSAGE(STATUS "Looking for HDF5 in Debian locations")
    FIND_PATH(HDF5_INCLUDE_DIR hdf5.h PATHS /usr/include/hdf5/serial)
    FIND_LIBRARY(HDF5 NAMES hdf5 PATHS /usr/lib/x86_64-linux-gnu/hdf5/serial)
    FIND_LIBRARY(HDF5_HL NAMES hdf5_hl PATHS /usr/lib/x86_64-linux-gnu/hdf5/serial)
ELSEIF (HDF5_ROOT)
    MESSAGE(STATUS "Looking for HDF5 in specified location: ${HDF5_ROOT}")
    FIND_PATH(HDF5_INCLUDE_DIR hdf5.h PATHS ${HDF5_ROOT})
    FIND_LIBRARY(HDF5 NAMES hdf5 PATHS ${HDF5_ROOT})
    FIND_LIBRARY(HDF5_HL NAMES hdf5_hl PATHS ${HDF5_ROOT})
ELSE ()
    MESSAGE(STATUS "Looking for HDF5 in standard locations")
    FIND_PATH(HDF5_INCLUDE_DIR hdf5.h PATHS /usr/include /usr/local/include)
    FIND_LIBRARY(HDF5 NAMES hdf5 PATHS /usr/lib /usr/local/lib)
    FIND_LIBRARY(HDF5_HL NAMES hdf5_hl /usr/lib /usr/local/lib)
ENDIF ()

IF (HDF5 AND HDF5_HL)
   SET(HDF5_LIBRARIES ${HDF5} ${HDF5_HL})
ELSE ()
   SET(HDF5_LIBRARIES "NOTFOUND")
ENDIF ()

IF (HDF5_INCLUDE_DIR AND HDF5_LIBRARIES)
   SET(HDF5_FOUND TRUE)
ENDIF ()

IF (NOT HDF5_FOUND AND HDF5_FIND_REQUIRED)
   MESSAGE(FATAL_ERROR "Could not find HDF5")
ENDIF ()

MARK_AS_ADVANCED(HDF5_INCLUDE_DIR HDF5_LIBRARIES)