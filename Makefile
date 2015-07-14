# I don't like autotools...
UNAME := $(shell uname -s)

SRCDIR = $(CURDIR)/src
INCDIR = $(CURDIR)/include
OBJDIR = $(CURDIR)/obj
BINDIR = $(CURDIR)/bin
DBGDIR = $(CURDIR)/*.dSYM

RM = rm -rf

$(OBJDIR):
	mkdir $@

$(BINDIR):
	mkdir $@

## Mac OS X Specific ###################################################
ifeq ($(UNAME), Darwin)
	SUPPORTED = 1

	OSXFUSE_ROOT = /usr/local
	
	OSXFUSE_INCDIR = $(OSXFUSE_ROOT)/include/osxfuse/fuse
	OSXFUSE_LIBDIR = $(OSXFUSE_ROOT)/lib
	
	OSXFUSE_CFLAGS = -I$(OSXFUSE_INCDIR) -L$(OSXFUSE_LIBDIR)
	OSXFUSE_CFLAGS += -D_FILE_OFFSET_BITS=64
	OSXFUSE_CFLAGS += -D_DARWIN_USE_64_BIT_INODE

	CFLAGS += $(OSXFUSE_CFLAGS)	
	LIBS += -losxfuse

	# GCC is usually symlinked to clang; for *real* GCC, install with,
	# e.g., homebrew, and define as /usr/local/bin/gcc
	CC = gcc
endif

## Linux Specific ######################################################
ifeq ($(UNAME), Linux)
	SUPPORTED = 1
endif

## Universal Build Rules ###############################################
ifdef SUPPORTED

TARGETS = 13amp

INCLUDES += -I$(INCDIR)

CFLAGS_EXTRA = -Wall -g
CFLAGS += $(CFLAGS_EXTRA) $(INCLUDES)

all: $(OBJDIR) $(BINDIR)
	@echo "Not done yet..."

clean:
	$(RM) $(OBJDIR) $(BINDIR) $(DBGDIR)

# Echo compiler and linker flags
print_cflags:
	@echo "Compiler flags: $(CFLAGS)"

print_ldflags:
	@echo "Linker flags: $(LDFLAGS)"

# Uh oh...
else

all:
	@>&2 echo "Unsupported platform: $(UNAME)" && exit 1

endif

.PHONY: all clean print_cflags print_ldflags
