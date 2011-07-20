#!/bin/bash

#export CC=gcc-3.3
#export CXX=g++-3.3

if [ "$HOSTNAME" = "FouDuBassan" ] ; then
  echo "Sur FouDuBassan"
  AS_src_path="/home/ycaniou/Recherche/0910/CSP_philippeCodognet/Git/Adaptive"
  AS_install_path="/home/ycaniou/Recherche/0910/CSP_philippeCodognet/Git/build"
fi
  
##############################################################################
# Create TAGS
if [ "x$1" == "xtags" ] ; then
  cd $AS_src_path
  if [ -e TAGS ] ; then
    echo "Remove TAGS file"
    rm TAGS
  fi
  echo "Generation of TAGS file"
  etags `find . -name '*.[ch]' -o -name '*.[ch][ch]'`
  cd -
fi

# Compile
if [ "x$1" == "xdconfig" ] ; then 
cmake $AS_src_path                                              \
  -DAS_USE_COMM_CONFIG:BOOL=ON                                  \
  -DAS_USE_MPI:BOOL=ON                                          \
  -DAS_USE_SEQ:BOOL=OFF                                         \
  -DAS_USE_ABORT:BOOL=ON                                        \
  -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON                              \
  -DCMAKE_INSTALL_PREFIX:PATH=$AS_install_path                  \
  -DAS_USE_DEBUG:BOOL=ON                                        \
  -DAS_PRINT_COSTS:BOOL=ON                                        \
  -DAS_PRINT_QUEUE:BOOL=ON                                        \
  -DCMAKE_C_FLAGS_INIT:STRING="-g -DSTATS"
elif [ "x$1" == "xconfig" ] ; then 
cmake $AS_src_path                                              \
  -DAS_USE_COMM_CONFIG:BOOL=ON                                  \
  -DAS_USE_MPI:BOOL=ON                                          \
  -DAS_USE_SEQ:BOOL=OFF                                         \
  -DAS_USE_ABORT:BOOL=ON                                        \
  -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON                              \
  -DCMAKE_INSTALL_PREFIX:PATH=$AS_install_path                  \
  -DAS_USE_DEBUG:BOOL=OFF
elif [ "x$1" == "xdmpi" ] ; then 
cmake $AS_src_path                                              \
  -DAS_USE_COMM_CONFIG:BOOL=OFF                                  \
  -DAS_USE_MPI:BOOL=ON                                          \
  -DAS_USE_SEQ:BOOL=OFF                                         \
  -DAS_USE_ABORT:BOOL=ON                                        \
  -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON                              \
  -DCMAKE_INSTALL_PREFIX:PATH=$AS_install_path                  \
  -DAS_USE_DEBUG:BOOL=ON                                        \
  -DAS_PRINT_COSTS:BOOL=ON                                        \
  -DAS_PRINT_QUEUE:BOOL=ON                                        \
  -DCMAKE_C_FLAGS_INIT:STRING="-g -DSTATS"
elif [ "x$1" == "xmpi" ] ; then 
cmake $AS_src_path                                              \
  -DAS_USE_COMM_CONFIG:BOOL=OFF                                  \
  -DAS_USE_MPI:BOOL=ON                                          \
  -DAS_USE_SEQ:BOOL=OFF                                         \
  -DAS_USE_ABORT:BOOL=ON                                        \
  -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON                              \
  -DCMAKE_INSTALL_PREFIX:PATH=$AS_install_path                  \
  -DAS_USE_DEBUG:BOOL=OFF
elif [ "x$1" == "xdseq" ] ; then 
cmake $AS_src_path                                              \
  -DAS_USE_COMM_CONFIG:BOOL=OFF                                 \
  -DAS_USE_MPI:BOOL=OFF                                         \
  -DAS_USE_SEQ:BOOL=ON                                          \
  -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON                              \
  -DCMAKE_INSTALL_PREFIX:PATH=$AS_install_path                  \
  -DAS_USE_DEBUG:BOOL=ON                                        \
  -DAS_PRINT_COSTS:BOOL=OFF                                        \
  -DCMAKE_C_FLAGS_INIT:STRING="-g -DSTATS=1"
elif [ "x$1" == "xseq" ] ; then 
cmake $AS_src_path                                              \
  -DAS_USE_COMM_CONFIG:BOOL=OFF                                  \
  -DAS_USE_MPI:BOOL=OFF                                          \
  -DAS_USE_SEQ:BOOL=ON                                         \
  -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON                              \
  -DCMAKE_INSTALL_PREFIX:PATH=$AS_install_path \
  -DAS_USE_DEBUG:BOOL=OFF
else
cmake $AS_src_path                                              \
  -DAS_USE_COMM_CONFIG:BOOL=ON                                  \
  -DAS_USE_MPI:BOOL=ON                                          \
  -DAS_USE_SEQ:BOOL=OFF                                         \
  -DAS_USE_ABORT:BOOL=ON                                        \
  -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON                              \
  -DCMAKE_INSTALL_PREFIX:PATH=$AS_install_path                  \
  -DAS_USE_DEBUG:BOOL=OFF
fi

#cmake $diet_src_path -DDIET_USE_BLAS:BOOL=ON                             \
#    $option                                                              \
#    -DCMAKE_CXX_FLAGS:STRING="-lpthread -g -D__linux__ -DYC_DEBUUG "       \
#    -DDIET_BUILD_EXAMPLES:BOOL=ON                                          \
#    -DDIET_WITH_STATISTICS:BOOL=ON                                         \
#    -DDIET_USE_ALT_BATCH:BOOL=ON                                          \
#    -DAEGIS_DIR:PATH=$path2aegis                                           \
#    -DDIET_USE_CORI:BOOL=ON                                               \
#    -DDIET_MAINTAINER_MODE:BOOL=ON                                       \
#    -DCMAKE_BUILD_TYPE:STRING=DEBUG                                     \
#    -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON                                   \
#    -DCMAKE_INSTALL_PREFIX:PATH=$diet_install_path

