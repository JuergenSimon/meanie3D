# -------------------------------------
# cmake configuration
# -------------------------------------

CMAKE_MINIMUM_REQUIRED(VERSION 3.2.0)
MARK_AS_ADVANCED(CMAKE_BACKWARDS_COMPATIBILITY)
SET(CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS TRUE)
SET(CMAKE_VERBOSE_MAKEFILE ON)
SET(CMAKE_MACOSX_RPATH OFF)

# -------------------------------------
# Project configuration
# -------------------------------------

PROJECT(meanie3D-python)
GET_DIRECTORY_PROPERTY(PACKAGE_VERSION DIRECTORY .. DEFINITION PACKAGE_VERSION)
GET_DIRECTORY_PROPERTY(PROJECT_URL DIRECTORY .. DEFINITION PROJECT_URL)
GET_DIRECTORY_PROPERTY(PROJECT_LICENSE DIRECTORY .. DEFINITION PROJECT_LICENSE)

# Python
FIND_PACKAGE (Python2 COMPONENTS Interpreter)
IF (Python2_FOUND)
    MESSAGE(STATUS "Found Python interpreter (${Python2_VERSION}) ${Python2_EXECUTABLE}")
ELSE ()
    MESSAGE(FATAL_ERROR "python not found")
ENDIF ()

# ------------------------------------------------------------- #
# Python packaging
# ------------------------------------------------------------- #
IF (Python2_FOUND)

    # Use the system's python path to append it to visit's python
    # path to enable importing modules to visit's python.
    EXECUTE_PROCESS(COMMAND ${Python2_EXECUTABLE} -c "import sys; sys.stdout.write(':'.join(sys.path))" OUTPUT_VARIABLE SYSTEM_PYTHON_PATH)

    # Configure package file
    FILE(COPY ${CMAKE_CURRENT_SOURCE_DIR}/meanie3D DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    FILE(COPY ${CMAKE_CURRENT_SOURCE_DIR}/setup.py.in DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    FILE(COPY ${CMAKE_CURRENT_SOURCE_DIR}/MANIFEST.in DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    FILE(COPY ${CMAKE_CURRENT_SOURCE_DIR}/README.txt.in DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

    SET(SETUP_PY ${CMAKE_CURRENT_BINARY_DIR}/setup.py)
    CONFIGURE_FILE(${CMAKE_CURRENT_BINARY_DIR}/setup.py.in ${SETUP_PY})
    CONFIGURE_FILE(${CMAKE_CURRENT_BINARY_DIR}/meanie3D/__init__.py.in ${CMAKE_CURRENT_BINARY_DIR}/meanie3D/__init__.py)
    CONFIGURE_FILE(${CMAKE_CURRENT_BINARY_DIR}/README.txt.in ${CMAKE_CURRENT_BINARY_DIR}/README.txt)

    ADD_CUSTOM_TARGET(copy-python-files ALL
            COMMAND cmake -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/meanie3D ${CMAKE_CURRENT_BINARY_DIR}
            DEPENDS meanie3D)
    ADD_CUSTOM_TARGET(${PROJECT_NAME} ALL COMMAND ${Python2_EXECUTABLE} setup.py build)
    ADD_CUSTOM_TARGET(${PROJECT_NAME}-clean COMMAND rm -rf build dist *.egg-info setup.py meanie3D/__init__.py README.txt)
    ADD_CUSTOM_TARGET(${PROJECT_NAME}-uninstall COMMAND ${Python2_EXECUTABLE} -m pip uninstall meanie3D)
    INSTALL(CODE "EXECUTE_PROCESS(COMMAND \"${Python2_EXECUTABLE}\" setup.py install WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})")
ENDIF ()
