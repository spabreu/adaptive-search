#############################################################################
# Adaptive search
#
#  Copyright (C) 2002-2010 Daniel Diaz, Philippe Codognet and Salvador Abreu 
#  Author:                                                               
#    - Yves Caniou (yves.caniou@ens-lyon.fr)                               
#      adapted from DIET project
#############################################################################
# Handle the specificities of the C compiler 
INCLUDE( ${AS_SOURCE_DIR}/Cmake/CheckCCompiler.cmake )

# Reset the values before testing:
SET( CMAKE_C_FLAGS ${CMAKE_C_FLAGS_INIT}
  CACHE STRING "Flags for C compiler"
  FORCE
  )

#####################################################################
###### Does the C compiler support inlining option ?
CHECK_C_COMPILER_SUPPORTS_INLINE( INLINE_VALUE INLINE_SUPPORTED )

### GCC does not implement ISO C99 semantics for inline functions:
# in contrast to the ISO C99 semantics, [for GNU C semantics] a function
# defined# as __inline__ provides an external definition only; a function
# defined as
# static __inline__ provides an inline definition with internal linkage (as
# in ISO C99)...
# For the details refer to:
# - http://gcc.gnu.org/onlinedocs/gcc-4.1.0/gcc/Inline.html#Inline
# - http://publib.boulder.ibm.com/infocenter/pseries/v5r3/topic/com.ibm.xlcpp8a.doc/language/ref/cplr243.htm
# HENCE, when the C compiler happens to be gcc we use "static __inline__"
# in place of "inline":
IF( CMAKE_COMPILER_IS_GNUCC )
  IF( NOT INLINE_SUPPORTED )
    MESSAGE( SEND_ERROR "Gcc should (and does indeed) support inlining ! " )
  ENDIF( NOT INLINE_SUPPORTED )
  SET( INLINE_VALUE "static __inline__" )
ENDIF( CMAKE_COMPILER_IS_GNUCC )

SET( AS_CMAKE_C_FLAGS "")
IF( INLINE_SUPPORTED )
  IF( NOT "${INLINE_VALUE}" MATCHES "^inline$" )
    FILE( APPEND ${CMAKE_BINARY_DIR}/CMakeOutput.log
          "Adding exotic C inline support: ${INLINE_VALUE}\n\n" )
    SET( AS_CMAKE_C_FLAGS "-Dinline=\"${INLINE_VALUE}\"")
  ENDIF( NOT "${INLINE_VALUE}" MATCHES "^inline$" )
ENDIF( INLINE_SUPPORTED )

#####################################################################
### Is the C compiler ANSI C-conforming for const ?
CHECK_C_COMPILER_SUPPORTS_CONST( CONST_SUPPORTED )
IF( NOT CONST_SUPPORTED )
  SET( AS_CMAKE_C_FLAGS "${AS_CMAKE_C_FLAGS} -Dconst=\"\"" )
ENDIF( NOT CONST_SUPPORTED )

### Pass the results to cmake internal flags:
IF( AS_CMAKE_C_FLAGS )
  SET( CMAKE_C_FLAGS
    "${CMAKE_C_FLAGS_INIT} ${AS_CMAKE_C_FLAGS}"
    CACHE
    STRING "Flags for C compiler"
    FORCE
  )
ENDIF( AS_CMAKE_C_FLAGS )

