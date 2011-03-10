INCLUDE_DIRECTORIES( ${CMAKE_CURRENT_SOURCE_DIR} )
# For later, if evolution... ${AS_SOURCE_DIR}/include )

set(AS_SOURCES
	ad_solver.c
	main.c
	tools.c
	no_cost_var.c
	no_exec_swap.c
	no_cost_swap.c
	no_next_i.c
	no_next_j.c
	no_displ_sol.c
	no_reset.c
)

IF (AS_USE_MPI)
   include_directories(${MPI_INCLUDE_PATH})
   set( AS_SOURCES
   	${AS_SOURCES}
	tools_MPI.c
	ad_solver_MPI.c
	)
ENDIF (AS_USE_MPI)

IF (AS_USE_CELL)
   set( AS_LIBRARIES
   	${AS_LIBRARIES}
	pthread
	)
ENDIF (AS_USE_CELL)

add_library(AS_LIB ${AS_SOURCES})
# INSTALL( TARGETS ${AS_LIB} DESTINATION lib )
# To update if/when project evolves with a doc/bin/lib structure
link_directories( ${CMAKE_CURRENT_BINARY_DIR} )

SET( PROBLEM_LIST
     queens
     perfect-square
     partit
     magic-square
     langford
     cap
     alpha
     all-interval
)

FOREACH( loop_var ${PROBLEM_LIST} )
  ADD_EXECUTABLE( ${loop_var} ${loop_var}.c )
  TARGET_LINK_LIBRARIES( ${loop_var}
                         libAS_LIB.a -lm )
  IF (AS_USE_MPI)
    TARGET_LINK_LIBRARIES( ${loop_var}
                           mpi )
  ENDIF (AS_USE_MPI)

#  INSTALL( TARGETS ${loop_var} DESTINATION bin )
ENDFOREACH( loop_var )

