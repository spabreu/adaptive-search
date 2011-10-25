CELL_TOP = $(HOME)/cell/sdk

vpath    %.c .:..

S        = ..
DEBUG    = 0
COMM	 = -UCELL_COMM
MBX      = -UAS_MBX

###############################################################################

PROGRAMS_spu := $(BENCH)
LIBRARY_embed := $(BENCH).a


NO_DEFAULTS = $(subst $(S)/,,$(wildcard $(S)/no_*.c))

DEFAULTS = $(NO_DEFAULTS) ad_solver.c tools.c spu.c
DEFAULTS_o := $(subst .c,.o,$(DEFAULTS))

OBJS_$(BENCH) := $(BENCH).o
OBJS := $(OBJS_$(BENCH))

IMPORTS := libdefaults.a

CFLAGS += -DDEBUG=$(DEBUG)
CFLAGS += $(COMM)
CFLAGS += $(MBX)
CFLAGS += -DCELL=1 -UAS_PPU -DAS_SPU=1
CFLAGS += -Wno-unused-parameter

###############################################################################

include $(CELL_TOP)/buildutils/make.footer



spu.o: ../cell-common.h

libdefaults.a: $(DEFAULTS_o)
	$(AR) -qcs $@ $(DEFAULTS_o)

realclean:
	rm -f *.a $(BENCH)
