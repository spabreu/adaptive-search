#############################################################################
# Adaptive search
#
#  Copyright (C) 2002-2010 Daniel Diaz, Philippe Codognet and Salvador Abreu 
#  Author:                                                               
#    - Yves Caniou (yves.caniou@ens-lyon.fr)                               
#      adapted from DIET project
#
# Display a little summary
#
#############################################################################

# Display a title line
function(disp )
  if (NOT DEFINED ARGV1)
	  set(char "X")
	else (NOT DEFINED ARGV1)
	  set(char ${ARGV1})
	endif (NOT DEFINED ARGV1)
	
  set(cmpt 0)
	set(resultStr "")
	
	if (DEFINED ARGV0)
    string(LENGTH ${ARGV0} len)
	else (DEFINED ARGV0)
	  set(len 0)
	endif (DEFINED ARGV0)
	
	if (${len} GREATER 0)
	  math(EXPR len "${len} + 4")
	endif(${len} GREATER 0)
	math(EXPR len "(76  - ${len})/2")
	
	while (${cmpt} LESS ${len})
	  set(resultStr ${resultStr}${char})
    math(EXPR cmpt "${cmpt}+1")
	endwhile (${cmpt} LESS ${len})
	
	if (DEFINED ARGV0)
	  string(LENGTH ${ARGV0} modulo)
		math(EXPR modulo "${modulo} % 2")
		if (${modulo} EQUAL 0)
      set(resultStr "${resultStr}  ${ARGV0}  ${resultStr}")
		else (${modulo} EQUAL 0)
  		set(resultStr "${resultStr}  ${ARGV0}  ${resultStr}${char}")
		endif (${modulo} EQUAL 0)
	else (DEFINED ARGV0)
    set(resultStr "${resultStr}${resultStr}")
	endif (DEFINED ARGV0)
	message(STATUS ${resultStr})
endfunction(disp)

disp()
disp("Adaptive Search ${AS_VERSION}")
disp("Configuration summary")
disp("${AS_BUILD_VERSION}")
disp()

if (CYGWIN)
  message(STATUS "XXX System name Cygwin on Windows")
elseif (APPLE)
  message(STATUS "XXX System name Darwin")
elseif (LINUX)
  message(STATUS "XXX System name Linux")
elseif (AIX)
  message(STATUS "XXX System name Aix")
elseif (SUNOS)
  message(STATUS "XXX System name SunOS")
elseif (FREEBSD)
  message(STATUS "XXX System name FreeBSD")
endif (CYGWIN)

message(STATUS "XXX - Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "XXX - C compiler   : ${CMAKE_C_COMPILER}")
IF (CMAKE_C_COMPILER_VERSION)
  message(STATUS "XXX    * version   : ${CMAKE_C_COMPILER_VERSION}")
ENDIF (CMAKE_C_COMPILER_VERSION)
message(STATUS "XXX    * options   : ${CMAKE_C_FLAGS}")
message(STATUS "XXX - CXX compiler : ${CMAKE_CXX_COMPILER}")
IF (CMAKE_CXX_COMPILER_VERSION)
  message(STATUS "XXX    *  version  : ${CMAKE_CXX_COMPILER_VERSION}")
ENDIF (CMAKE_CXX_COMPILER_VERSION)
message(STATUS "XXX    *  options  : ${CMAKE_CXX_FLAGS}")

message(STATUS "XXX - Options set: ")
IF (AS_USE_MPI)
  message(STATUS "XXX * MPI found: ${MPI_FOUND}")
  message(STATUS "XXX   -- MPI compiler: ${MPI_COMPILER}")
  message(STATUS "XXX   -- MPI version: ${MPI_VERSION}")
  message(STATUS "XXX   -- MPI directory: ${MPI_DIR}")
  message(STATUS "XXX   -- MPI includes: ${MPI_INCLUDE_DIR}")
  message(STATUS "XXX   -- MPI libraries:")
	foreach (lib ${MPI_LIBRARIES})
	  message(STATUS "XXX     - ${lib}")
	endforeach (lib)
ENDIF (AS_USE_MPI)

disp()
