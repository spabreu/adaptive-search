PKG_NAME=adaptive-$(shell cat ../VERSION)


ifndef DEBUG
DEBUG = 0
endif

ifdef COMM
MBX = -DAS_MBX
COMM = -DCELL_COMM
else
MBX = -UAS_MBX
COMM = -UCELL_COMM
endif

CC=gcc

#CFLAGS=-g -Wall -DDEBUG -DLOG_FILE
#CFLAGS=-g -Wall -DDEBUG
#CFLAGS=-fomit-frame-pointer -O3 -DLOG_FILE -Wall
CFLAGS=-fomit-frame-pointer -O3 -W -Wall -Wno-unused-parameter \
	-DDEBUG=$(DEBUG) $(COMM) $(MBX)

# for profiling

#CFLAGS=-g -Wall -DDEBUG -DLOG_FILE -pg
#CFLAGS=-O3 -pg -Wall

RANLIB=ranlib


OBJLIB = ad_solver.o tools.o main.o \
	 no_cost_var.o no_exec_swap.o no_cost_swap.o \
	 no_next_i.o no_next_j.o no_displ_sol.o

LIBNAME=libad_solver.a

EXECS=magic-square queens alpha all-interval partit langford perfect-square

%: %.c $(LIBNAME)
	$(CC) -o $@ $(CFLAGS) $< $(LIBNAME)

%-cell: spu/%.a Makefile.cell
	make -f Makefile.cell \
		DEBUG=$(DEBUG) MBX=$(MBX) COMM=$(COMM) \
			libdefaults.a
	make -f Makefile.cell \
		DEBUG=$(DEBUG) MBX=$(MBX) COMM=$(COMM) \
			BENCH=$(@:-cell=)

spu/%.a: %.c
	cd spu; make COMM=$(COMM) MBX=$(MBX) DEBUG=$(DEBUG) BENCH=$*


all: $(EXECS)

cell: $(patsubst %,%-cell,$(EXECS))

$(LIBNAME): $(OBJLIB)
	rm -f $(LIBNAME) 
	ar -rc $(LIBNAME) $(OBJLIB)
	$(RANLIB) $(LIBNAME)


ad_solver.o: ad_solver.h

tools.o: tools.h


# distribution

ROOT_DIR=$(shell cd ..;pwd)
DST=./$(PKG_NAME)

distdir=/tmp
distpath=$(distdir)/$(PKG_NAME)

dist: check_doc $(PKG_NAME).tar.gz

check_doc:
	(cd ../doc; make all)

$(PKG_NAME).tar.gz: copy_dist_tree
	(cd $(distdir); tar cf $(ROOT_DIR)/src/$(PKG_NAME).tar $(PKG_NAME))
	gzip -f $(PKG_NAME).tar
	-rm -rf $(distpath)
	@banner="$(PKG_NAME).tar.gz is ready for distribution"; \
	dashes=`echo "$$banner" | sed s/./=/g`; \
	echo "$$dashes"; \
	echo "$$banner"; \
	echo "$$dashes"


# below we use 2 cd in case distpath is not an absolute path like '.'
copy_dist_tree:
	-rm -rf $(distpath)
	mkdir -p $(distpath)
	while read f;do files="$$files $$f"; done <DISTRIB_FILES; \
	(cd ..;tar cf - $$files | (cd $(ROOT_DIR)/src;cd $(distpath); tar xf -))



BENCH=$(patsubst %,BENCH-RESULT-%.txt,$(EXECS))

# create SQL files from txt
%.sql: %.txt Makefile
	sed -e 's/[ 	][ 	]*/,/g' < $< > $@

BENCH.sql: $(BENCH:.txt=.sql)
	cat $+ > $@

# cleaning

clean:
	cd spu; make clean realclean; rm -f $(EXECS) 
	rm -f *.o *.a *.d *~ $(EXECS) *-spu-thread.*
