#############################################################################
# Adaptive search
#
#  Copyright (C) 2002-2010 Daniel Diaz, Philippe Codognet and Salvador Abreu 
#  Author:                                                               
#    - Yves Caniou (yves.caniou@ens-lyon.fr)                               
#      adapted from DIET project
#############################################################################
# Caveat emptor: according to the thread 
#    http://public.kitware.com/pipermail/cmake/2006-April/008795.html
# cmake's current (2.2.x) abilities to define a Gui/Command-line overridable
# build mode (or customized built-in mode default compiler flags or an extended 
# build mode) are limited.
#
# FOR THE TIME BEING ALL THE DEFAULTS AND EXTENSIONS SET HERE ARE NOT
# OVERRIDABLE EITHER FROM THE COMMAND LINE NOR FROM THE GUIs.
# Hence if you need to change some of those defaults you'll get to
# edit this file and manage things manually... Sorry for that!
#
################# OFFER AN EXTRA MAINTAINER BUILD MODE #######################
# The extra maintainer build type is a build mode for which the compilers
# and linkers go paranoid and report about most of the warning they are aware
# of.
# Notes:
#  - Limitation: this Maintainer mode is only available for GCC.
#  - Comments on the current choice of flags:
#     * -Wold-style-cast generates too many warnings due to omniORB includes
#     * -pedantic generates ISO C++ unsupported 'long long' errors for omniORB
#        includes
#     * -Wstrict-null-sentinel used only when supported :-)

SET( CMAKE_BUILD_TYPE_DOCSTRING
  "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel." )

IF( CMAKE_COMPILER_IS_GNUCC AND CMAKE_COMPILER_IS_GNUCXX )
  # Some warning flags (e.g. -Wstrict-null-sentinel) were introduced only
  # recently within GCC. In order to decide wether those flags are available
  # or not we simply check g++ major version number (as opposed to heavier
  # solutions like making trial invocations and checking the result). Hence
  # the following definition of GNUCXX_MAJOR_VERSION_BIGER_THAN_FOUR):
  EXEC_PROGRAM( ${CMAKE_CXX_COMPILER}
               ARGS -dumpversion
               OUTPUT_VARIABLE GNUCXX_VERSION )
  STRING(COMPARE GREATER ${GNUCXX_VERSION} "4.0.0"
    GNUCXX_MAJOR_VERSION_BIGER_THAN_FOUR
  )

  # Allthough we made sure that the compilers (C and CXX) were the GNU ones,
  # we alas cannot expect the linker to be GNU-ld (as usually packaged in
  # binutils). For example on Darwin, one encounters c++ as being a
  # tweaked version of g++, but ld is not GNU-ld (but some Apple's cctools
  # based thingy).
  # Since cmake doesn't seem to offer a CMAKE_LINKER_IS_GNU (i.e.
  # the equivalent of CMAKE_COMPILER_IS_GNUCC for the linker) we need to
  # provide one. Alas (again) the following strategy fails:
  #         EXEC_PROGRAM( ${CMAKE_CXX_LINK_EXECUTABLE}
  #                        ARGS -v
  #                        OUTPUT_VARIABLE LINKER_VERSION )
  #         IF( LINKER_VERSION MATCHES "GNU" )
  #             SET( LINKER_IS_GNULD TRUE )
  #         ENDIF( ...)
  # Failure is due to the fact that the linker command is not accessible
  # from cmake since it is invoked through the compilers (in fact 
  # CMAKE_CXX_LINK_EXECUTABLE is a rule and not the linker command).
  #
  # Hence the following kludgy definition of LINKER_IS_GNULD (which reveals
  # that proprietary OSes prove to remain hard to use even when they pretend
  # to deliver the GNU layer):
  SET( LINKER_IS_GNULD TRUE )
  IF( APPLE )
    SET( LINKER_IS_GNULD FALSE )
    SET ( CMAKE_CXX_FLAGS_MAINTAINER "-D__darwin__" )
  ENDIF( APPLE )

  # Proceed with the definitions:
  IF( GNUCXX_MAJOR_VERSION_BIGER_THAN_FOUR )
    SET( CMAKE_CXX_FLAGS_MAINTAINER
       "-Wall -Wabi -Woverloaded-virtual -Wstrict-null-sentinel"
     )
  ELSE( GNUCXX_MAJOR_VERSION_BIGER_THAN_FOUR )
    SET( CMAKE_CXX_FLAGS_MAINTAINER "-Wall -Wabi -Woverloaded-virtual" )
  ENDIF( GNUCXX_MAJOR_VERSION_BIGER_THAN_FOUR )

  SET( CMAKE_CXX_FLAGS_MAINTAINER
    ${CMAKE_CXX_FLAGS_MAINTAINER}
    CACHE STRING 
    "Flags used by the C++ compiler during maintainer builds."
    FORCE
  )

  SET( CMAKE_C_FLAGS_MAINTAINER
    "-Wall -pedantic -fomit-frame-pointer -Wno-unused-parameter"
    CACHE STRING 
    "Flags used by the C compiler during maintainer builds."
    FORCE
  )

  IF( LINKER_IS_GNULD )
    IF( GNUCXX_MAJOR_VERSION_BIGER_THAN_FOUR )
      IF( CYGWIN )
        SET( CMAKE_EXE_LINKER_FLAGS_MAINTAINER
        "-Wl,--unresolved-symbols=report-all,--warn-unresolved-symbols,--warn-once --enable-auto-import --export-dynamic -Wl,--export-dynamic,--enable-auto-import"
        )
      ELSE( CYGWIN )
        SET( CMAKE_EXE_LINKER_FLAGS_MAINTAINER
        "-Wl,--unresolved-symbols=report-all,--warn-unresolved-symbols,--warn-once"
        )
      ENDIF( CYGWIN )
    ELSE( GNUCXX_MAJOR_VERSION_BIGER_THAN_FOUR )
      IF( CYGWIN )
        SET( CMAKE_EXE_LINKER_FLAGS_MAINTAINER "-Wl,--warn-once --enable-auto-import --export-dynamic -Wl,--export-dynamic,--enable-auto-import" )
      ELSE( CYGWIN )
        SET( CMAKE_EXE_LINKER_FLAGS_MAINTAINER "-Wl,--warn-once" )
      ENDIF( CYGWIN)
    ENDIF( GNUCXX_MAJOR_VERSION_BIGER_THAN_FOUR )
  ELSE( LINKER_IS_GNULD )
      SET( CMAKE_EXE_LINKER_FLAGS_MAINTAINER "" )
  ENDIF( LINKER_IS_GNULD )
  SET( CMAKE_EXE_LINKER_FLAGS_MAINTAINER
    ${CMAKE_EXE_LINKER_FLAGS_MAINTAINER}
    CACHE STRING 
    "Flags used for linking binaries during maintainer builds."
    FORCE
  )

  SET( CMAKE_SHARED_LINKER_FLAGS_MAINTAINER
    ${CMAKE_EXE_LINKER_FLAGS_MAINTAINER}
    CACHE STRING 
    "Flags used by the shared libraries linker during maintainer builds."
    FORCE
  )

  SET( CMAKE_MODULE_LINKER_FLAGS_MAINTAINER
    "-dummy_option_to_see_what_happens"
    CACHE STRING 
    "What the hack is a module anyhow (Apple notion?)..."
    FORCE
  )

  MARK_AS_ADVANCED(
    CMAKE_CXX_FLAGS_MAINTAINER
    CMAKE_C_FLAGS_MAINTAINER
    CMAKE_EXE_LINKER_FLAGS_MAINTAINER
    CMAKE_SHARED_LINKER_FLAGS_MAINTAINER
    CMAKE_MODULE_LINKER_FLAGS_MAINTAINER
  )

  SET( CMAKE_BUILD_TYPE_DOCSTRING
    "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel Maintainer." )
ENDIF( CMAKE_COMPILER_IS_GNUCC AND CMAKE_COMPILER_IS_GNUCXX )

################# MODIFY THE DEFAULT OF DEBUG MODE #######################
# When using GCC compiler collection, the Debug build-in mode type (refer
# to CMAKE_BUILD_TYPE) is modified to be defined as the Maintainer mode
# with an additional -g flag.
IF( CMAKE_COMPILER_IS_GNUCC AND CMAKE_COMPILER_IS_GNUCXX )
  SET( CMAKE_CXX_FLAGS_DEBUG
    "${CMAKE_CXX_FLAGS_DEBUG_INIT} ${CMAKE_CXX_FLAGS_MAINTAINER}"
    CACHE STRING 
    "Flags used by the C++ compiler during debug builds."
    FORCE
  )
  SET( CMAKE_C_FLAGS_DEBUG
    "${CMAKE_C_FLAGS_DEBUG_INIT} ${CMAKE_C_FLAGS_MAINTAINER}"
    CACHE STRING 
    "Flags used by the C compiler during debug builds."
    FORCE
  )
  SET( CMAKE_EXE_LINKER_FLAGS_DEBUG
    "${CMAKE_EXE_LINKER_FLAGS_DEBUG_INIT} ${CMAKE_EXE_LINKER_FLAGS_MAINTAINER}"
    CACHE STRING 
    "Flags used for linking binaries during debug builds."
    FORCE
  )
  SET( CMAKE_SHARED_LINKER_FLAGS_DEBUG
    "${CMAKE_SHARED_LINKER_FLAGS_DEBUG_INIT} ${CMAKE_SHARED_LINKER_FLAGS_MAINTAINER}"
    CACHE STRING 
    "Flags used by the shared libraries linker during debug builds."
    FORCE
  )
  SET( CMAKE_MODULE_LINKER_FLAGS_DEBUG
    "${CMAKE_MODULE_LINKER_FLAGS_DEBUG_INIT} ${CMAKE_MODULE_LINKER_FLAGS_MAINTAINER}"
    CACHE STRING 
    "What the hack is a module anyhow (Apple notion?)..."
    FORCE
  )
ENDIF( CMAKE_COMPILER_IS_GNUCC AND CMAKE_COMPILER_IS_GNUCXX )

############################################################################
# Default the build mode to RelWithDebInfo [compile c++ and c code with
#    optimizations and debug info i.e.e roughly speaking "-O2 -g" on Un*x]:
IF( NOT CMAKE_BUILD_TYPE )
  SET( CMAKE_BUILD_TYPE
    RelWithDebInfo CACHE STRING
    ${CMAKE_BUILD_TYPE_DOCSTRING}
    FORCE
  )
ENDIF(NOT CMAKE_BUILD_TYPE)
