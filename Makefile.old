TARGETS = bin/13amp 
OSXFUSE_ROOT = /usr/local

INCLUDE_DIR  = $(OSXFUSE_ROOT)/include/osxfuse/fuse
LIBRARY_DIR  = $(OSXFUSE_ROOT)/lib

CC = gcc # Symlinked to clang
# CC = /usr/local/bin/gcc-5 # Actual GCC

CFLAGS_OSXFUSE = -I$(INCLUDE_DIR) -L$(LIBRARY_DIR)
CFLAGS_OSXFUSE += -D_FILE_OFFSET_BITS=64
CFLAGS_OSXFUSE += -D_DARWIN_USE_64_BIT_INODE

CFLAGS_EXTRA = -Wall -g $(CFLAGS)

CFLAGS += -Iinclude/

LIBS = -losxfuse

bin/13amp: src/13amp.c src/13amp_ops.c
	$(CC) $(CFLAGS_OSXFUSE) $(CFLAGS_EXTRA) $(CFLAGS) -o $@ $+  $(LIBS)

all: $(TARGETS)


clean:
	rm -f $(TARGETS) *.o
	rm -rf *.dSYM

## # I don't like autotools...
## UNAME := $(shell uname -s)
## 
## SRCDIR = $(CURDIR)/src
## INCDIR = $(CURDIR)/include
## OBJDIR = $(CURDIR)/obj
## BINDIR = $(CURDIR)/bin
## DBGDIR = $(CURDIR)/*.dSYM
## 
## RM = rm -rf
## 
## $(OBJDIR):
## 	mkdir $@
## 
## $(BINDIR):
## 	mkdir $@
## 
## ## Mac OS X Specific ###################################################
## ifeq ($(UNAME), Darwin)
## 	SUPPORTED = 1
## 
## 	OSXFUSE_ROOT = /usr/local
## 	
## 	OSXFUSE_INCDIR = $(OSXFUSE_ROOT)/include/osxfuse/fuse
## 	OSXFUSE_LIBDIR = $(OSXFUSE_ROOT)/lib
## 	
## 	OSXFUSE_CFLAGS = -I$(OSXFUSE_INCDIR) -L$(OSXFUSE_LIBDIR)
## 	OSXFUSE_CFLAGS += -D_FILE_OFFSET_BITS=64
## 	OSXFUSE_CFLAGS += -D_DARWIN_USE_64_BIT_INODE
## 
## 	CFLAGS += $(OSXFUSE_CFLAGS)	
## 	LIBS += -losxfuse
## 
## 	# GCC is usually symlinked to clang; for *real* GCC, install with,
## 	# e.g., homebrew, and define as /usr/local/bin/gcc
## 	CC = gcc
## endif
## 
## ## Linux Specific ######################################################
## ifeq ($(UNAME), Linux)
## 	SUPPORTED = 1
## endif
## 
## ## Universal Build Rules ###############################################
## ifdef SUPPORTED
## 
## TARGETS = 13amp
## 
## BINS = $(BINDIR)/13amp
## OBJS = $(OBJDIR)/13amp.o
## 
## LIBOBJS = $(OBJDIR)/13amp_ops.o
## 
## INCLUDES += -I$(INCDIR)
## 
## CFLAGS_EXTRA = -Wall -g
## CFLAGS += $(CFLAGS_EXTRA) $(INCLUDES)
## 
## all: $(LIBOBJS) $(OBJS) $(BINS)
## 	@true
## 
## $(OBJDIR)/%.o: $(SRCDIR)/%.c
## 	$(CC) -c $(CFLAGS) -o $@ $<
## 
## $(BINDIR)/%: $(OBJDIR)/%.o $(LIBOBJS)
## 	$(LDR) -o $@ $< $(LIBOBJS) $(LDFLAGS)
## 
## clean:
## 	$(RM) $(OBJDIR) $(BINDIR) $(DBGDIR)
## 
## # Echo compiler and linker flags
## print_cflags:
## 	@echo "Compiler flags: $(CFLAGS)"
## 
## print_ldflags:
## 	@echo "Linker flags: $(LDR) $(LDFLAGS)"
## 
## # Uh oh...
## else
## 
## all:
## 	@>&2 echo "Unsupported platform: $(UNAME)" && exit 1
## 
## endif
## 
## .PHONY: all clean print_cflags print_ldflags
