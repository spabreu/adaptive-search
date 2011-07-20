#############################################################################
# Adaptive search
#
#  Copyright (C) 2002-2010 Daniel Diaz, Philippe Codognet and Salvador Abreu 
#  Author:                                                               
#    - Yves Caniou (yves.caniou@ens-lyon.fr)                               
#      adapted from DIET project
#############################################################################
project(AS)
cmake_minimum_required(VERSION 2.6)

set(AS_MAJOR_VERSION 1)
set(AS_MINOR_VERSION 1)

set(AS_VERSION ${AS_MAJOR_VERSION}.${AS_MINOR_VERSION})

# Get Date
if (WIN32)
  execute_process(COMMAND "date" "+%Y/%m/%d-%H:%M:%S" 
  			  OUTPUT_VARIABLE BUILD_DATE)
  string(STRIP ${BUILD_DATE} BUILD_DATE) # get rid of newline
elseif (UNIX)
  execute_process(COMMAND "date" "+%Y/%m/%d-%H:%M:%S" 
  			  OUTPUT_VARIABLE BUILD_DATE)
  string(STRIP ${BUILD_DATE} BUILD_DATE) # get rid of newline
else (WIN32)
  message(SEND_ERROR "date not implemented")
	set(${BUILD_DATE} 000000)
endif (WIN32)

set(AS_BUILD_VERSION ${BUILD_DATE})

# ---------------------------------------------------------------------------
# Seq => !MPI && !CELL ++ MPI => !Seq && !CELL ++ CELL => !Seq && !MPI 

# Save compiler, in case of MPI/Seq/Cell modifications and undo
SET(CMAKE_C_COMPILER_SAV ${CMAKE_C_COMPILER} )
SET(CMAKE_CXX_COMPILER_SAV ${CMAKE_CXX_COMPILER} )

if (AS_USE_SEQ)
  if (AS_USE_MPI)
    set(AS_USE_MPI OFF CACHE BOOL "MPI support" FORCE)
    # Restore compiler
    SET(CMAKE_C_COMPILER ${CMAKE_C_COMPILER_SAV} )
    SET(CMAKE_CXX_COMPILER ${CMAKE_CXX_COMPILER_SAV} )
    message(WARNING " MPI support desactivated for sequential support.")
  endif (AS_USE_MPI)
  if (AS_USE_CELL)
    set(AS_USE_CELL OFF CACHE BOOL "CELL support" FORCE)
    message(WARNING " CELL support desactivated for sequential support.")
  endif (AS_USE_CELL)
endif (AS_USE_SEQ)

if (AS_USE_MPI)
  if (AS_USE_SEQ)
    set(AS_USE_SEQ OFF CACHE BOOL "Sequential support" FORCE)
    message(WARNING " Sequential support desactivated for MPI support.")
  endif (AS_USE_SEQ)
  if (AS_USE_CELL)
    set(AS_USE_CELL OFF CACHE BOOL "CELL support" FORCE)
    message(WARNING " CELL support desactivated for MPI support.")
  endif (AS_USE_CELL)
endif (AS_USE_MPI)

# At least one choice validated
IF (NOT AS_USE_SEQ)
   IF (NOT AS_USE_MPI)
      IF (NOT AS_USE_CELL)
         set(AS_USE_SEQ ON CACHE BOOL "Sequential support" FORCE)
   	 message(WARNING " Sequential support activated by default.")
      ENDIF (NOT AS_USE_CELL)
   ENDIF (NOT AS_USE_MPI)
ENDIF (NOT AS_USE_SEQ)

IF ( AS_USE_STATS )
  set(AS_PRINT_COST ON CACHE BOOL "Print cost" FORCE)
ENDIF ( AS_USE_STATS )

option(AS_USE_MPI              	"Build AS with MPI support." OFF)
option(AS_USE_CELL             	"Build AS with Cell support." OFF)
option(AS_USE_SEQ              	"Build AS in sequential." ON)
option(AS_USE_DEBUG            	"Build AS with Debug mode." OFF)
option(AS_USE_ABORT            	"Build AS with Abort termination." OFF)
option(AS_USE_BARRIER          	"Build AS with Barrier Sync. beginning." OFF)
option(AS_USE_BACKTRACK		"Build AS with backtracking when reset." OFF)
option(AS_USE_COMM_CONFIG	"Build AS with communication of config." OFF)
option(AS_USE_STATS		"Build AS with statistics on." OFF)
option(AS_PRINT_COSTS		"Build AS with COST printing support." OFF)
option(AS_PRINT_QUEUE		"Build AS with MPI Msg queue printing support." OFF)

# --------------------- DEPENDENCIES TOWARDS "CLASSIC" C++ HEADERS ---------
# This is just a convenience to advance the detection of any potential missing
# C++ "classic" header to the cmake stage. If we were to forget the following
# tests (and such a header was missing), things would simply break at
# preprocessing stage of the compilation...
# [Those tests are a translation of the AC_CHECK_HEADER(...) that were expressed
# in the configure.ac of the autotools version].
include(${CMAKE_ROOT}/Modules/CheckIncludeFileCXX.cmake)

SET (AS_INCLUDES_TO_CHECK
  stdio.h
  stdlib.h
  string.h
  stdarg.h
  assert.h
  iostream
  limits.h
  math.h
  unistd.h
  sys/stat.h
  sys/types.h
)

IF ( AS_USE_CELL )
   SET ( AS_INCLUDES_TO_CHECK
   	${AS_INCLUDES_TO_CHECK}
	malloc.h
	)
ENDIF ( AS_USE_CELL )

IF ( AS_USE_MPI )
   SET ( AS_INCLUDES_TO_CHECK
   	${AS_INCLUDES_TO_CHECK}
	time.h
	sys/time.h
	limits.h
	)
# TODO: PRINT_COSTS
    IF ( AS_USE_PRINT_COST )
        SET ( AS_INCLUDES_TO_CHECK
   	      ${AS_INCLUDES_TO_CHECK}
	      fcntl.h
	      errno.h
	      )
    ENDIF ( AS_USE_PRINT_COST )
ENDIF ( AS_USE_MPI )

foreach (include_to_check ${AS_INCLUDES_TO_CHECK})
  CHECK_INCLUDE_FILE_CXX( ${include_to_check} dummyfound${include_to_check} )
  if (NOT dummyfound${include_to_check})
    message(FATAL_ERROR "Missing ${include_to_check} header file.")
  endif (NOT dummyfound${include_to_check})
endforeach (include_to_check)

# --------------------- DEPENDENCIES TOWARDS SYSTEM FUNCTIONS ------------
# This is just a convenience to advance the detection of any potential missing
# external function to the cmake stage. If we were to forget the following
# tests (and such an external function was missing), things would simply break
# at linking stage...
# [Those tests are a translation of the AC_CHECK_FUNCS(...) that were expressed
# in the configure.ac of the autotools version].
INCLUDE(${CMAKE_ROOT}/Modules/CheckFunctionExists.cmake)

set(AS_FUNCTIONS_TO_CHECK
  printf
  time
  sleep
  strlen
  gethostname
  gettimeofday
  strdup
)

foreach(function_to_check ${AS_FUNCTIONS_TO_CHECK})
  CHECK_FUNCTION_EXISTS( ${function_to_check} dummyfound${function_to_check} )
  if (NOT dummyfound${function_to_check})
    message(FATAL_ERROR "Missing ${function_to_check} function.")
  endif (NOT dummyfound${function_to_check})
endforeach (function_to_check)

#-------------- PLATFORM SPECIFIC COMPILATION FLAGS -----------------------
# Requires CMake >= 2.4.7
# Only tested on Linux...

string(COMPARE EQUAL ${CMAKE_SYSTEM_NAME} "AIX" AIX)
string(COMPARE EQUAL ${CMAKE_SYSTEM_NAME} "Darwin" APPLE)
string(COMPARE EQUAL ${CMAKE_SYSTEM_NAME} "Linux" LINUX)
string(COMPARE EQUAL ${CMAKE_SYSTEM_NAME} "SunOS" SUNOS)
string(COMPARE EQUAL ${CMAKE_SYSTEM_NAME} "FreeBSD" FREEBSD)
string(COMPARE EQUAL ${CMAKE_SYSTEM_NAME} "CYGWIN" CYGWIN)

if (CYGWIN)
  add_definitions(-D__cygwin__)
elseif (AIX)
  add_definitions(-D__aix__)
elseif (APPLE)
  add_definitions( -D__darwin__)
elseif (LINUX)
  add_definitions(-D__linux__)
elseif (SUNOS)
  add_definitions(-D__sunos__)
elseif(FREEBSD)
  add_definitions(-D__freebsd__)
endif (CYGWIN)

# ---------------------------------------------------------------------------
# Find mpi
IF ( AS_USE_MPI )
  find_package(MPI)
  IF ( MPI_FOUND )
    get_filename_component(_MPI_BASE_DIR ${MPI_COMPILER} PATH)
    find_program(MPI_COMPILER_CXX
	         NAMES mpic++ mpicxx mpiCC
 	         HINTS "${_MPI_BASE_DIR}"
 	         PATH_SUFFIXES bin
 	         DOC "MPI compiler. Used only to detect MPI compilation flags.")

    find_program(MPI_COMPILER_CC
	         NAMES mpicc
 	         HINTS "${_MPI_BASE_DIR}"
         	 PATH_SUFFIXES bin
         	 DOC "MPI compiler. Used only to detect MPI compilation flags.")

#    set(USE_MPI_COMPILER_CONFIG
#        -DCMAKE_C_COMPILER:PATH=${MPI_COMPILER_CC}
#        -DCMAKE_CXX_COMPILER:PATH=${MPI_COMPILER_CXX})
#     endif()
#     mark_as_advanced(MPI_COMPILER_CC MPI_COMPILER_CXX)
#     set(titan_mpi_args
#       -DMPI_LIBRARY:FILEPATH=${MPI_LIBRARY}
#       -DMPI_INCLUDE_PATH:PATH=${MPI_INCLUDE_PATH})
#     if(MPI_EXTRA_LIBRARY)
#       list(APPEND titan_mpi_args
#         -DMPI_EXTRA_LIBRARY:FILEPATH=${MPI_EXTRA_LIBRARY}
#         )
#     endif()
#   endif()

     # change compiler
     SET(CMAKE_C_COMPILER ${MPI_COMPILER_CC})
     SET(CMAKE_CXX_COMPILER ${MPI_COMPILER_CXX})
#    MESSAGE(WARN "MPI installation found.")
  ELSE ( MPI_FOUND )
#  IF ( NOT MPI_FOUND )
    MESSAGE("MPI installation was not found. Please provide MPI_DIR:")
    MESSAGE("  - through the GUI when working with ccmake, ")
    MESSAGE("  - as a command line argument when working with cmake e.g. ")
    MESSAGE("    cmake .. -DMPI_DIR:PATH=/usr/local/mpi/")
    MESSAGE("Note: the following message is triggered by cmake on the first ")
    MESSAGE("    undefined necessary PATH variable (e.g.  MPI_INCLUDE_DIR).")
    MESSAGE("    Providing MPI_DIR (as above described) is probably the")
    MESSAGE("    simplest solution unless you have a really customized/odd")
    MESSAGE("    mpi installation...")
    SET( MPI_DIR "" CACHE PATH "Root of MPI install tree.")
  ENDIF ( MPI_FOUND )
#  ENDIF ( NOT MPI_FOUND )

#   set( USE_MPI_COMPILER_CONFIG )
#   IF ( UNIX )
#   # Find the same compiler as specified by the find MPI module
#   IF ( NOT EXISTS ${MPI_COMPILER} )
#     message(FATAL_ERROR "You must set MPI_COMPILER.")
#   ENDIF ( NOT EXISTS ${MPI_COMPILER} )

ENDIF ( AS_USE_MPI )


# --------------- OFFER BUILD MODE EXTENSIONS AND DEFAULTS  ----------------
INCLUDE( ${AS_SOURCE_DIR}/Cmake/ASDefaultBuildTypes.cmake )

# --------------- HANDLE SPECIFICITIES OF THE C COMPILER -------------------
INCLUDE( ${AS_SOURCE_DIR}/Cmake/ConfigureCCompiler.cmake )

#---------------- COMPILER SPECIFIC COMPILATION FLAGS ----------------------

IF( CMAKE_COMPILER_IS_GNUCC )
  # Get version
  EXEC_PROGRAM(${CMAKE_C_COMPILER} ARGS "-dumpversion"
                                   OUTPUT_VARIABLE CMAKE_C_COMPILER_VERSION )
  EXEC_PROGRAM(${CMAKE_CXX_COMPILER} ARGS "-dumpversion"
                                     OUTPUT_VARIABLE CMAKE_CXX_COMPILER_VERSION)
  # Use ISO 99C
  SET( CMAKE_C_FLAGS ${CMAKE_C_FLAGS} " -std=gnu99" )
  STRING( REPLACE ";" " " CMAKE_C_FLAGS ${CMAKE_C_FLAGS} ) # transform previous list in a string
ENDIF( CMAKE_COMPILER_IS_GNUCC )

# ---------------- SET THE LINKER SEARCH PATH (RPATH) ----------------------
#SET( CMAKE_INSTALL_RPATH_USE_LINK_PATH ON
#     CACHE BOOL "Whether to set an rpath for dynamic libraries." )
#SET( CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib
#     CACHE STRING "Rpath set at install stage." FORCE )
#SET(CMAKE_INSTALL_NAME_DIR ${CMAKE_INSTALL_PREFIX}/lib)

# ------------------ PROJECT DEFINITION ---------------------

IF( AS_USE_DEBUG )
   IF( AS_USE_MPI )
     add_definitions(-DDEBUG=1 -DDEBUG_MPI=1 -Wall -fPIC)
   ELSE( AS_USE_MPI )
     add_definitions(-DDEBUG=1 -Wall -fPIC)
   ENDIF( AS_USE_MPI )
ELSE (AS_USE_DEBUG)
    add_definitions(-UDEBUG)
ENDIF (AS_USE_DEBUG)

if (AS_USE_SEQ)
    add_definitions(-UDELL_COMM)
endif (AS_USE_SEQ)

if (AS_USE_CELL)
   add_definitions(-DCELL_COMM)
endif (AS_USE_CELL)

if (AS_USE_MPI) 
   add_definitions(-DMPI=1)
endif (AS_USE_MPI)

if (AS_PRINT_COSTS) 
   add_definitions(-DPRINT_COSTS=1)
endif (AS_PRINT_COSTS)

if (AS_USE_ABORT) 
   add_definitions(-DMPI_ABORT=1)
endif (AS_USE_ABORT)

if (AS_USE_BARRIER) 
   add_definitions(-DMPI_BEGIN_BARRIER=1)
endif (AS_USE_BARRIER)

if (AS_USE_BACKTRACK)
   add_definitions(-DBACKTRACK=1)
endif (AS_USE_BACKTRACK)

if (AS_USE_COMM_CONFIG)
   if (AS_USE_SEQ)
      message(FATAL_ERROR "Communication can only be done with non seq code.")
   endif (AS_USE_SEQ)
   add_definitions(-DCOMM_CONFIG=1)
endif (AS_USE_COMM_CONFIG)

# if (AS_USE_COMM_CONFIG_BACKTRACK)
#    if (AS_USE_SEQ)
#       message(FATAL_ERROR "Communication can only be done with non seq code.")
#    endif (AS_USE_SEQ)
#    if (NOT AS_USE_BACKTRACK)
#         message(WARNING "Communicating needs backtracking code: turned on.")
#         set(AS_USE_BACKTRACK ON CACHE BOOL "Backtracking reset" FORCE)
#    endif (NOT AS_USE_BACKTRACK)
#    add_definitions(-DCOMM_CONFIG=1)
# endif (AS_USE_COMM_CONFIG_BACKTRACK)

if (AS_USE_STATS)
   add_definitions(-DSTATS=1)
endif (AS_USE_STATS)

if (AS_PRINT_QUEUE)
   if( NOT AS_USE_MPI )
     message(FATAL_ERROR "PRINT_QUEUE can only be set in MPI mode.")
   endif( NOT AS_USE_MPI )
   if( NOT AS_USE_DEBUG )
     message(FATAL_ERROR "PRINT_QUEUE can only be set in DEBUG mode.")
   endif( NOT AS_USE_DEBUG )
   add_definitions(-DDEBUG_PRINT_QUEUE=1 -DDEBUG_QUEUE=1 -DDEBUG_MPI_ENDING=1)
endif (AS_PRINT_QUEUE)



# --------------------------------------------------------------------------
add_subdirectory(src)
add_subdirectory(Cmake)
# --------------------------------------------------------------------------
include(${AS_SOURCE_DIR}/Cmake/DisplaySummary.cmake)

## eof - CMakeLists.txt
